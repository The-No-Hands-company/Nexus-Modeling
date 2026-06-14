import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { IDEEngine } from "./ide-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3084");
  const baseUrl = process.env.NEXUS_NEXUS_IDE_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new IDEEngine("data/ide.sqlite")
  const phantom = new PhantomApp("nexus-ide");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-ide", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-ide", status: "ready", capabilities: ["ide", "code-editor", "ai-assistance"], cloudIntegration: { enabled: (process.env["NEXUS_IDE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/ide/projects") return json(engine.listProjects(), 200);
      if (request.method === "POST" && path === "/api/v1/ide/projects") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createProject(b.name, b.language || ""), 201);
      }
      const pm = path.match(/^\/api\/v1\/ide\/projects\/([^/]+)$/);
      if (request.method === "GET" && pm) { const p = engine.getProject(pm[1]!); return p ? json(p, 200) : json({ error: "not found" }, 404); }
      const fm = path.match(/^\/api\/v1\/ide\/projects\/([^/]+)\/files$/);
      if (request.method === "GET" && fm) return json(engine.listFiles(fm[1]!), 200);
      if (request.method === "POST" && fm) {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.addFile(fm[1]!, b.name, b.content || "", b.language || ""), 201);
      }
      const fu = path.match(/^\/api\/v1\/ide\/files\/([^/]+)$/);
      if (request.method === "PUT" && fu) {
        const b = await request.json().catch(() => ({})) as any;
        if (b.content == null) return json({ error: "content required" }, 400);
        const f = engine.updateFile(fu[1]!, b.content); return f ? json(f, 200) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-ide] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
