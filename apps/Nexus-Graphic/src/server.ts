import { randomUUID } from "node:crypto";
import { GraphicEngine } from "./graphic-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3080");
  const startedAt = Date.now();
  const engine = new GraphicEngine("data/nexus-graphic.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-graphic", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/graphic/projects") return json(engine.listProjects());
if (req.method === "POST" && p === "/api/v1/graphic/projects") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createProject(b.name, b.width, b.height), 201); }
const gpm = p.match(/^\/api\/v1\/graphic\/projects\/([^/]+)$/); if (req.method === "GET" && gpm) { const prj = engine.getProject(gpm[1]!); return prj ? json(prj) : json({ error: "not found" }, 404); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-graphic] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
