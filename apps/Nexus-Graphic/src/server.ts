import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud"; import { GraphicEngine } from "./graphic-engine";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3080");
  const baseUrl = process.env.NEXUS_NEXUS_GRAPHIC_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new GraphicEngine("data/nexus-graphic.sqlite");

  const phantom = new PhantomApp("nexus-graphic");
  const phantomId = await phantom.start();

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-graphic", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), phantom: phantom.status() });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-graphic", status: "ready", capabilities: ["graphic-design", "vector", "raster", "ai-generation"], cloudIntegration: { enabled: (process.env["NEXUS_GRAPHIC_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      if (req.method === "GET" && p === "/api/v1/graphic/projects") return json(engine.listProjects());
if (req.method === "POST" && p === "/api/v1/graphic/projects") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createProject(b.name, b.width, b.height), 201); }
const gpm = p.match(/^\/api\/v1\/graphic\/projects\/([^/]+)$/); if (req.method === "GET" && gpm) { const prj = engine.getProject(gpm[1]!); return prj ? json(prj) : json({ error: "not found" }, 404); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-graphic] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, phantom, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
