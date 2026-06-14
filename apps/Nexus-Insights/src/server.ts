import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { InsightsEngine } from "./insights-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3086");
  const baseUrl = process.env.NEXUS_NEXUS_INSIGHTS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new InsightsEngine("data/insights.sqlite")
  const phantom = new PhantomApp("nexus-insights");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-insights", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-insights", status: "ready", capabilities: ["insights", "ai-analysis", "reporting"], cloudIntegration: { enabled: (process.env["NEXUS_INSIGHTS_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/insights/datasets") return json(engine.listDatasets(), 200);
      if (request.method === "POST" && path === "/api/v1/insights/datasets") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name || !b.columns) return json({ error: "name and columns required" }, 400);
        return json(engine.createDataset(b.name, b.columns), 201);
      }
      if (request.method === "GET" && path === "/api/v1/insights/reports") return json(engine.listReports(), 200);
      if (request.method === "POST" && path === "/api/v1/insights/reports") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title || !b.type || !b.dataset) return json({ error: "title, type, dataset required" }, 400);
        return json(engine.createReport(b.title, b.type, b.dataset, b.config), 201);
      }
      const rm = path.match(/^\/api\/v1\/insights\/reports\/([^/]+)$/);
      if (request.method === "GET" && rm) { const r = engine.getReport(rm[1]!); return r ? json(r, 200) : json({ error: "not found" }, 404); }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-insights] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
