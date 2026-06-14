import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { PlayEngine } from "./play-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3098"); const baseUrl = process.env.NEXUS_NEXUS_PLAY_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new PlayEngine("data/play.sqlite")
  const phantom = new PhantomApp("nexus-play");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-play", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-play", status: "ready", capabilities: ["games","sessions","arcade"], cloudIntegration: { enabled: (process.env["NEXUS_PLAY_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/play/games") { const g = url.searchParams.get("genre") || undefined; return json(engine.listGames(g), 200); }
    if (request.method === "POST" && path === "/api/v1/play/games") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addGame(b.name, b.genre || "", b.maxPlayers || 4), 201); }
    if (request.method === "GET" && path === "/api/v1/play/sessions") { const gid = url.searchParams.get("gameId") || undefined; const st = url.searchParams.get("status") || undefined; return json(engine.listSessions(gid, st), 200); }
    if (request.method === "POST" && path === "/api/v1/play/sessions") { const b = await request.json().catch(() => ({})) as any; if (!b.gameId || !b.players) return json({ error: "gameId and players required" }, 400); return json(engine.startSession(b.gameId, b.players), 201); }
    if (request.method === "POST" && path === "/api/v1/play/sessions/end") { const b = await request.json().catch(() => ({})) as any; if (!b.id) return json({ error: "id required" }, 400); engine.endSession(b.id, b.score || 0, b.duration || 0); return json({ ok: true }, 200); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-play] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
