import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { NewsEngine } from "./news-engine";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
    },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3145");
  const baseUrl =
    process.env.NEXUS_NEWS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new NewsEngine("data/nexus-news.sqlite");

  const phantom = new PhantomApp("nexus-news");
  const phantomId = await phantom.start();

  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-news",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-news",
            status: "ready",
            capabilities: ["news"],
            cloudIntegration: {
              enabled:
                (process.env["NEXUS_NEWS_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200,
        );

      if (req.method === "GET" && p === "/api/v1/news/newses")
        return json(engine.listNewss());

      if (req.method === "POST" && p === "/api/v1/news/newses") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createNews(b.name, b.type), 201);
      }

      const gm = p.match(/^\/api\/v1\/news\/newses\/([^/]+)$/);
      if (req.method === "GET" && gm) {
        const entity = engine.getNews(gm[1]!);
        return entity ? json(entity) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-news] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    phantom,
    discovery,
    close: () => { stopHeartbeat(); phantom.stop(); server.stop(); },
  };
}
