import { randomUUID } from "node:crypto";
import { GameEngine } from "./game-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3079");
  const startedAt = Date.now();
  const engine = new GameEngine("data/nexus-game.sqlite")
  const phantom = new PhantomApp("nexus-game");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-game", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/game/servers") { const game = url.searchParams.get("game") || undefined; return json(engine.listServers(game)); }
if (req.method === "POST" && p === "/api/v1/game/servers") { const b = await req.json().catch(()=>({})) as any; if (!b.name || !b.game || !b.host || !b.port) return json({ error: "name, game, host, port required" }, 400); return json(engine.registerServer({ name: b.name, game: b.game, host: b.host, port: b.port, maxPlayers: b.maxPlayers }), 201); }
const gm = p.match(/^\/api\/v1\/game\/servers\/([^/]+)$/);
if (req.method === "POST" && gm && p.endsWith("/status")) { const b = await req.json().catch(()=>({})) as any; engine.updateStatus(gm[1]!, b.status || "online", b.players); return json({ updated: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-game] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
