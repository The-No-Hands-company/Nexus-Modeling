import { randomUUID } from "node:crypto";
import { ConfidentialEngine } from "./confidential-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3070");
  const baseUrl = process.env["NEXUS_NEXUS_CONFIDENTIAL_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new ConfidentialEngine();

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-confidential", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-confidential", status: "ready" });
      
      if (req.method === "POST" && p === "/api/v1/confidential/encrypt") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.data) return json({ error: "data required" }, 400);
        return json(engine.encrypt(b.data as string));
      }
      if (req.method === "POST" && p === "/api/v1/confidential/decrypt") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.encrypted) return json({ error: "encrypted required" }, 400);
        return json(engine.decrypt(b.encrypted as string));
      }
      if (req.method === "POST" && p === "/api/v1/confidential/hash") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.data) return json({ error: "data required" }, 400);
        return json({ hash: engine.hash(b.data as string) });
      }
      if (req.method === "GET" && p === "/api/v1/confidential/keys") return json({ keys: engine.listKeys() });
      if (req.method === "POST" && p === "/api/v1/confidential/keys") { return json(engine.generateKey()); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-confidential] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
