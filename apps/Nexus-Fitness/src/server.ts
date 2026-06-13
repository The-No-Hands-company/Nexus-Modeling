import { randomUUID } from "node:crypto";
import { FitnessEngine } from "./fitness-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3077");
  const startedAt = Date.now();
  const engine = new FitnessEngine("data/nexus-fitness.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-fitness", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "POST" && p === "/api/v1/fitness/workouts") { const b = await req.json().catch(()=>({})) as any; if (!b.userId || !b.type) return json({ error: "userId and type required" }, 400); return json(engine.logWorkout({ userId: b.userId, type: b.type, durationMinutes: b.durationMinutes||0, calories: b.calories||0, exercises: Array.isArray(b.exercises) ? b.exercises : [] }), 201); }
if (req.method === "GET" && p === "/api/v1/fitness/workouts") { const uid = url.searchParams.get("userId") || "default"; return json({ workouts: engine.listWorkouts(uid), stats: engine.getStats(uid) }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-fitness] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
