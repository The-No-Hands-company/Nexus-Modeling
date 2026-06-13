import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { HomeEngine } from "./home-engine";

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
  const port = Number(process.env.PORT || "3082");
  const baseUrl = process.env.NEXUS_NEXUS_HOME_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new HomeEngine("data/home.sqlite");

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-home", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-home", status: "ready", capabilities: ["smart-home", "iot", "automation"], cloudIntegration: { enabled: (process.env["NEXUS_HOME_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" } }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/home/rooms") return json(engine.listRooms(), 200);
      if (request.method === "POST" && path === "/api/v1/home/rooms") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createRoom(b.name), 201);
      }
      if (request.method === "GET" && path === "/api/v1/home/devices") {
        const roomId = url.searchParams.get("roomId") || undefined;
        return json(engine.listDevices(roomId), 200);
      }
      if (request.method === "POST" && path === "/api/v1/home/devices") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.roomId || !b.name || !b.type) return json({ error: "roomId, name, type required" }, 400);
        return json(engine.addDevice(b.roomId, b.name, b.type), 201);
      }
      const dm = path.match(/^\/api\/v1\/home\/devices\/([^/]+)\/toggle$/);
      if (request.method === "POST" && dm) { const d = engine.toggleDevice(dm[1]!); return d ? json(d, 200) : json({ error: "not found" }, 404); }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-home] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); server.stop(); } };
}
