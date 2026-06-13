import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { HealthEngine } from "./health-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      ...headers,
    },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3081");
  const baseUrl = process.env.NEXUS_NEXUS_HEALTH_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new HealthEngine("data/health.sqlite");

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({
          service: "nexus-health",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          timestamp: new Date().toISOString(),
        }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/status") {
        return json({
          service: "nexus-health",
          status: "ready",
          capabilities: ["health", "records", "ai-insights"],
          cloudIntegration: {
            enabled: (process.env["NEXUS_HEALTH_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/health/records") return json(engine.listRecords(), 200);
      if (request.method === "POST" && path === "/api/v1/health/records") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.metric || b.value == null) return json({ error: "metric and value required" }, 400);
        return json(engine.createRecord(b.metric, b.value, b.unit || "", b.notes || "", b.recordedAt), 201);
      }
      const rm = path.match(/^\/api\/v1\/health\/records\/([^/]+)$/);
      if (request.method === "GET" && rm) { const r = engine.getRecord(rm[1]!); return r ? json(r, 200) : json({ error: "not found" }, 404); }
      if (request.method === "GET" && path === "/api/v1/health/goals") return json(engine.listGoals(), 200);
      if (request.method === "POST" && path === "/api/v1/health/goals") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.metric || !b.target || !b.deadline) return json({ error: "metric, target, deadline required" }, 400);
        return json(engine.createGoal(b.metric, b.target, b.unit || "", b.deadline), 201);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-health] Listening on port ${server.port}`);

  const stopHeartbeat = startHeartbeat(baseUrl);

  return {
    server,
    engine,
    close: () => { stopHeartbeat(); server.stop(); },
  };
}
