import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { MeetEngine } from "./meet-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3090");
  const baseUrl = process.env.NEXUS_NEXUS_MEET_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new MeetEngine("data/meet.sqlite")
  const phantom = new PhantomApp("nexus-meet");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-meet", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-meet", status: "ready", capabilities: ["video-conferencing", "recording", "ai-transcription"], cloudIntegration: { enabled: (process.env["NEXUS_MEET_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/meet/rooms") return json(engine.listRooms(), 200);
      if (request.method === "POST" && path === "/api/v1/meet/rooms") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createRoom(b.name, b.maxParticipants), 201);
      }
      if (request.method === "POST" && path === "/api/v1/meet/rooms/join") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.roomId || !b.name) return json({ error: "roomId and name required" }, 400);
        const p = engine.joinRoom(b.roomId, b.name);
        return p ? json(p, 200) : json({ error: "room full or not found" }, 409);
      }
      const ptm = path.match(/^\/api\/v1\/meet\/rooms\/([^/]+)\/participants$/);
      if (request.method === "GET" && ptm) return json(engine.listParticipants(ptm[1]!), 200);
      if (request.method === "GET" && path === "/api/v1/meet/schedules") return json(engine.listSchedules(), 200);
      if (request.method === "POST" && path === "/api/v1/meet/schedules") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title || !b.roomId || !b.startTime) return json({ error: "title, roomId, startTime required" }, 400);
        return json(engine.scheduleMeeting(b.title, b.roomId, b.startTime, b.duration), 201);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-meet] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
