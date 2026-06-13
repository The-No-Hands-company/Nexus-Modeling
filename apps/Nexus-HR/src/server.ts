import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { HREngine } from "./hr-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3083");
  const baseUrl = process.env.NEXUS_NEXUS_HR_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new HREngine("data/hr.sqlite");

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-hr", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-hr", status: "ready", capabilities: ["hr", "onboarding", "payroll"], cloudIntegration: { enabled: (process.env["NEXUS_HR_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" } }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/hr/departments") return json(engine.listDepartments(), 200);
      if (request.method === "POST" && path === "/api/v1/hr/departments") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createDepartment(b.name), 201);
      }
      if (request.method === "GET" && path === "/api/v1/hr/employees") {
        const dept = url.searchParams.get("department") || undefined;
        return json(engine.listEmployees(dept), 200);
      }
      if (request.method === "POST" && path === "/api/v1/hr/employees") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name || !b.email) return json({ error: "name and email required" }, 400);
        return json(engine.createEmployee(b.name, b.email, b.department || "", b.position || "", b.salary || 0), 201);
      }
      const em = path.match(/^\/api\/v1\/hr\/employees\/([^/]+)$/);
      if (request.method === "GET" && em) { const e = engine.getEmployee(em[1]!); return e ? json(e, 200) : json({ error: "not found" }, 404); }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-hr] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); server.stop(); } };
}
