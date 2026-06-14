import { randomUUID } from "node:crypto";
import { AutomateEngine } from "./automate-engine";

function json(p: unknown, s = 200): Response { return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8" } }); }

export async function createServer() {
  const port = Number(process.env.PORT || "3065");
  const startedAt = Date.now();
  const engine = new AutomateEngine("data/automate.sqlite")
  const phantom = new PhantomApp("nexus-automate");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-automate", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });

      if (req.method === "GET" && p === "/api/v1/automate/workflows") return json({ workflows: engine.listWorkflows() });
      if (req.method === "POST" && p === "/api/v1/automate/workflows") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.name || !b.trigger || !b.actions) return json({ error: "name, trigger, actions required" }, 400);
        return json(engine.createWorkflow(b.name as string, b.trigger as any, b.actions as any), 201);
      }
      const wfMatch = p.match(/^\/api\/v1\/automate\/workflows\/([^/]+)$/);
      if (req.method === "GET" && wfMatch) {
        const w = engine.getWorkflow(wfMatch[1]!);
        return w ? json({ workflow: w, runs: engine.getRunLog(wfMatch[1]!) }) : json({ error: "not found" }, 404);
      }
      if (req.method === "POST" && wfMatch && p.endsWith("/execute")) {
        const result = await engine.executeWorkflow(wfMatch[1]!);
        return json({ executed: true, ...result });
      }
      if (req.method === "POST" && wfMatch && p.endsWith("/toggle")) {
        const w = engine.getWorkflow(wfMatch[1]!);
        if (!w) return json({ error: "not found" }, 404);
        engine.setEnabled(wfMatch[1]!, !w.enabled);
        return json({ enabled: !w.enabled });
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-automate] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
