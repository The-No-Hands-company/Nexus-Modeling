import { randomUUID } from "node:crypto";
import { ComplianceEngine } from "./compliance-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3031");
  const baseUrl = process.env["NEXUS_NEXUS_COMPLIANCE_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new ComplianceEngine("data/compliance.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-compliance", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-compliance", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/compliance/events") return json({ events: engine.listEvents() });
      if (req.method === "POST" && p === "/api/v1/compliance/events") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.type || !b.description) return json({ error: "type and description required" }, 400);
        return json(engine.recordEvent({ type: b.type as string, description: b.description as string, severity: (b.severity as string) || "info", source: (b.source as string) || "api", data: b.data as Record<string,unknown> }), 201);
      }
      if (req.method === "GET" && p === "/api/v1/compliance/report") {
        return json({ report: engine.generateReport() });
      }
      if (req.method === "GET" && p === "/api/v1/compliance/rules") return json({ rules: engine.listRules() });
      if (req.method === "POST" && p === "/api/v1/compliance/rules") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.name || !b.regulation) return json({ error: "name and regulation required" }, 400);
        return json(engine.createRule({ name: b.name as string, regulation: b.regulation as string, description: b.description as string, required: b.required as boolean } as any), 201);
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-compliance] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
