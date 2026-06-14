import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { CalendarEngine } from "./calendar-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3068");
  const baseUrl = process.env["NEXUS_NEXUS_CALENDAR_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new CalendarEngine("data/calendar.sqlite")
  const phantom = new PhantomApp("nexus-calendar");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-calendar", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-calendar", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/calendar/events") {
        const from = url.searchParams.get("from") || new Date().toISOString().slice(0,7)+"-01";
        const to = url.searchParams.get("to") || new Date(Date.now()+30*86400000).toISOString().slice(0,10);
        return json({ events: engine.listEvents(from, to) });
      }
      if (req.method === "POST" && p === "/api/v1/calendar/events") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.startTime || !b.endTime) return json({ error: "title, startTime, endTime required" }, 400);
        return json(engine.createEvent({ title: b.title as string, description: b.description as string, location: b.location as string, startTime: b.startTime as string, endTime: b.endTime as string, allDay: b.allDay as boolean, recurrence: b.recurrence as string }), 201);
      }
      const evMatch = p.match(/^\/api\/v1\/calendar\/events\/([^/]+)$/);
      if (req.method === "DELETE" && evMatch) { return engine.deleteEvent(evMatch[1]!) ? json({ deleted: true }) : json({ error: "not found" }, 404); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-calendar] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
