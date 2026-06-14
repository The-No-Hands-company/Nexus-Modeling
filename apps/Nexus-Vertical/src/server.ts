import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { VerticalEngine } from "./vertical-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3112"); const baseUrl = process.env.NEXUS_NEXUS_VERTICAL_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new VerticalEngine("data/vertical.sqlite")
  const phantom = new PhantomApp("nexus-vertical");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-vertical", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-vertical", status: "ready", capabilities: ["verticals","pipelines","stages","crm"], cloudIntegration: { enabled: (process.env["NEXUS_VERTICAL_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/vertical") { const ind = url.searchParams.get("industry") || undefined; return json(engine.listVerticals(ind), 200); }
    if (request.method === "POST" && path === "/api/v1/vertical") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addVertical(b.name, b.description || "", b.industry || ""), 201); }
    if (request.method === "GET" && path === "/api/v1/vertical/pipelines") { const vid = url.searchParams.get("verticalId"); if (!vid) return json({ error: "verticalId required" }, 400); return json(engine.listPipelines(vid), 200); }
    if (request.method === "POST" && path === "/api/v1/vertical/pipelines") { const b = await request.json().catch(() => ({})) as any; if (!b.verticalId || !b.name) return json({ error: "verticalId and name required" }, 400); return json(engine.addPipeline(b.verticalId, b.name, b.stages || "[]"), 201); }
    if (request.method === "GET" && path === "/api/v1/vertical/stages") { const pid = url.searchParams.get("pipelineId"); if (!pid) return json({ error: "pipelineId required" }, 400); return json(engine.listStages(pid), 200); }
    if (request.method === "POST" && path === "/api/v1/vertical/stages") { const b = await request.json().catch(() => ({})) as any; if (!b.pipelineId || !b.name) return json({ error: "pipelineId and name required" }, 400); return json(engine.addStage(b.pipelineId, b.name, b.order || 0, b.dealValue || 0), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-vertical] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
