import { randomUUID } from "node:crypto";
import { DesignEngine } from "./design-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3074");
  const startedAt = Date.now();
  const engine = new DesignEngine("data/nexus-design.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-design", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/design/projects") return json(engine.listProjects());
if (req.method === "POST" && p === "/api/v1/design/projects") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createProject(b.name, b.type || "web"), 201); }
const dp = p.match(/^\/api\/v1\/design\/projects\/([^/]+)$/); if (req.method === "GET" && dp) { const pr = engine.getProject(dp[1]!); return pr ? json(pr) : json({ error: "not found" }, 404); }
if (req.method === "POST" && dp && p.endsWith("/frames")) { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); engine.addFrame(dp[1]!, b.name); return json({ added: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-design] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
