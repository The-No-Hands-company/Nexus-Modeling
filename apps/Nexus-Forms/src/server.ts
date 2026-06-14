import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { FormsEngine } from "./forms-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3078");
  const startedAt = Date.now();
  const engine = new FormsEngine("data/nexus-forms.sqlite")
  const phantom = new PhantomApp("nexus-forms");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-forms", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/forms") return json(engine.listForms());
if (req.method === "POST" && p === "/api/v1/forms") { const b = await req.json().catch(()=>({})) as any; if (!b.title || !Array.isArray(b.fields)) return json({ error: "title and fields required" }, 400); return json(engine.createForm(b.title, b.fields), 201); }
const fm = p.match(/^\/api\/v1\/forms\/([^/]+)$/); if (req.method === "GET" && fm) { const f = engine.getForm(fm[1]!); return f ? json(f) : json({ error: "not found" }, 404); }
if (req.method === "POST" && fm && p.endsWith("/responses")) { const b = await req.json().catch(()=>({})) as any; if (!b.data) return json({ error: "data required" }, 400); return json(engine.submitResponse(fm[1]!, b.data), 201); }
if (req.method === "GET" && fm && p.endsWith("/responses")) return json(engine.getResponses(fm[1]!));
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-forms] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
