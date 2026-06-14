import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { MediaEngine } from "./media-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3089");
  const baseUrl = process.env.NEXUS_NEXUS_MEDIA_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new MediaEngine("data/media.sqlite")
  const phantom = new PhantomApp("nexus-media");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-media", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-media", status: "ready", capabilities: ["media", "streaming", "library"], cloudIntegration: { enabled: (process.env["NEXUS_MEDIA_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/media/albums") return json(engine.listAlbums(), 200);
      if (request.method === "POST" && path === "/api/v1/media/albums") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createAlbum(b.name, b.description), 201);
      }
      if (request.method === "GET" && path === "/api/v1/media/assets") {
        const albumId = url.searchParams.get("albumId") || undefined;
        return json(engine.listAssets(albumId), 200);
      }
      if (request.method === "POST" && path === "/api/v1/media/assets") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name || !b.type || !b.url) return json({ error: "name, type, url required" }, 400);
        return json(engine.addAsset(b.name, b.type, b.size || 0, b.url, b.tags, b.albumId), 201);
      }
      const am = path.match(/^\/api\/v1\/media\/assets\/([^/]+)$/);
      if (request.method === "GET" && am) { const a = engine.getAsset(am[1]!); return a ? json(a, 200) : json({ error: "not found" }, 404); }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-media] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
