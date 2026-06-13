import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { MindEngine } from "./mind-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3091");
  const baseUrl = process.env.NEXUS_NEXUS_MIND_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new MindEngine("data/mind.sqlite");

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-mind", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-mind", status: "ready", capabilities: ["wellness", "journal", "meditation"], cloudIntegration: { enabled: (process.env["NEXUS_MIND_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" } }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/mind/maps") return json(engine.listMaps(), 200);
      if (request.method === "POST" && path === "/api/v1/mind/maps") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title) return json({ error: "title required" }, 400);
        return json(engine.createMap(b.title, b.description), 201);
      }
      const mm = path.match(/^\/api\/v1\/mind\/maps\/([^/]+)$/);
      if (request.method === "GET" && mm) { const m = engine.getMap(mm[1]!); return m ? json(m, 200) : json({ error: "not found" }, 404); }
      const nm = path.match(/^\/api\/v1\/mind\/maps\/([^/]+)\/nodes$/);
      if (request.method === "GET" && nm) return json(engine.listNodes(nm[1]!), 200);
      if (request.method === "POST" && nm) {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.label) return json({ error: "label required" }, 400);
        return json(engine.addNode(nm[1]!, b.label, b.content || "", b.parentId || "", b.x || 0, b.y || 0), 201);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-mind] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); server.stop(); } };
}
