// Nexus Systems — Production Reverse Proxy
// Routes subdomains to backend apps. Handles CORS and WebSocket upgrades.
//
// Routes:
//   nexussystems.vexr.dev/       → status page
//   cloud.nexussystems.vexr.dev  → Nexus-Cloud (port 8787)
//   chat.nexussystems.vexr.dev   → Nexus Team Chat (port 3109)

const PORT = Number(process.env.PROXY_PORT || "80");
const CLOUD_UPSTREAM = process.env.CLOUD_UPSTREAM || "http://127.0.0.1:8787";
const CHAT_UPSTREAM = process.env.CHAT_UPSTREAM || "http://127.0.0.1:3109";
const DOMAIN = process.env.DOMAIN || "nexussystems.vexr.dev";

// Simple wildcard matching: host ends with domain
function routeHost(host: string): keyof typeof routes {
  if (!host) return "main";
  if (host.startsWith("cloud.")) return "cloud";
  if (host.startsWith("chat.")) return "chat";
  return "main";
}

const routes = {
  main: "http://127.0.0.1:9000", // built-in status page below
  cloud: CLOUD_UPSTREAM,
  chat: CHAT_UPSTREAM,
};

async function handleRequest(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const host = url.hostname;
  const target = routes[routeHost(host)];

  // CORS preflight
  if (req.method === "OPTIONS") {
    return new Response(null, {
      status: 204,
      headers: {
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, PATCH, OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type, Authorization, X-API-Key",
      },
    });
  }

  // Status page for root domain
  if (routeHost(host) === "main") {
    return statusPage();
  }

  // Proxy to upstream
  try {
    const upstream = new URL(req.url);
    upstream.hostname = new URL(target).hostname;
    upstream.port = new URL(target).port;

    const proxied = new Request(upstream.toString(), {
      method: req.method,
      headers: req.headers,
      body: req.method !== "GET" && req.method !== "HEAD" ? await req.arrayBuffer() : undefined,
    });

    const resp = await fetch(proxied);
    const headers = new Headers(resp.headers);
    headers.set("Access-Control-Allow-Origin", "*");
    return new Response(resp.body, { status: resp.status, headers });
  } catch (err) {
    return new Response(`Upstream unreachable: ${target}`, { status: 502 });
  }
}

function statusPage(): Response {
  return new Response(
    `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Nexus Systems</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: system-ui, -apple-system, sans-serif; background: #0a0a1a; color: #e0e0f0; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .card { background: #12122a; border: 1px solid #2a2a4a; border-radius: 12px; padding: 48px; max-width: 520px; width: 100%; text-align: center; }
    h1 { font-size: 2rem; margin-bottom: 8px; color: #7c5cfc; }
    .tagline { color: #888; margin-bottom: 32px; }
    .services { display: grid; gap: 12px; }
    .service { background: #16163a; border: 1px solid #2a2a5a; border-radius: 8px; padding: 16px; text-align: left; display: flex; justify-content: space-between; align-items: center; }
    .service .name { font-weight: 600; }
    .service .url { font-size: 0.85rem; color: #7c5cfc; }
    .status { width: 10px; height: 10px; border-radius: 50%; background: #22c55e; }
    .footer { margin-top: 32px; font-size: 0.8rem; color: #555; }
    a { color: #7c5cfc; text-decoration: none; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Nexus Systems</h1>
    <p class="tagline">Sovereign, self-hosted ecosystem</p>
    <div class="services">
      <a href="https://cloud.${DOMAIN}" class="service">
        <span class="name">Nexus Cloud</span>
        <span class="url">cloud.${DOMAIN}</span>
        <span class="status"></span>
      </a>
      <a href="https://chat.${DOMAIN}" class="service">
        <span class="name">Nexus Team Chat</span>
        <span class="url">chat.${DOMAIN}</span>
        <span class="status"></span>
      </a>
    </div>
    <p class="footer">nexussystems.vexr.dev — Free. Open source. Privacy-first.</p>
  </div>
</body>
</html>`,
    { status: 200, headers: { "Content-Type": "text/html" } }
  );
}

console.log(`[proxy] Starting on port ${PORT}`);
console.log(`[proxy] Domain: ${DOMAIN}`);
console.log(`[proxy] Cloud → ${CLOUD_UPSTREAM}`);
console.log(`[proxy] Chat  → ${CHAT_UPSTREAM}`);

const server = Bun.serve({ port: PORT, fetch: handleRequest });
console.log(`[proxy] Listening on ${server.hostname}:${server.port}`);
