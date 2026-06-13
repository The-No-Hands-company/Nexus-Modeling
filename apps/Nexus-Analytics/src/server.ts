import { randomUUID } from "node:crypto";
import { AnalyticsEngine } from "./analytics-engine";
import { startNexusAnalyticsHeartbeat } from "./cloud";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3063");
  const baseUrl = process.env.NEXUS_ANALYTICS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new AnalyticsEngine();

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-analytics", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });

      if (req.method === "POST" && p === "/api/v1/analytics/track") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.name || b.value === undefined) return json({ error: "name and value required" }, 400);
        engine.track(b.name as string, b.value as number, (b.unit as string) || "count", (b.source as string) || "api");
        return json({ tracked: true }, 201);
      }

      if (req.method === "GET" && p === "/api/v1/analytics/query") {
        const name = url.searchParams.get("name");
        if (!name) return json({ error: "name query param required" }, 400);
        const window = Number(url.searchParams.get("window") || "24");
        return json({ metrics: engine.query(name, window), aggregate: engine.aggregate(name) });
      }

      if (req.method === "GET" && p === "/api/v1/analytics/metrics") return json({ metrics: engine.listMetrics() });

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-analytics] Listening on port ${server.port}`);
  const stop = startNexusAnalyticsHeartbeat(baseUrl);
  return { server, close: () => { stop(); server.stop(); } };
}
