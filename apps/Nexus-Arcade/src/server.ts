import { randomUUID } from "node:crypto";
import { ArcadeEngine } from "./arcade-engine";

function json(p: unknown, s = 200): Response { return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8" } }); }

export async function createServer() {
  const port = Number(process.env.PORT || "3064");
  const startedAt = Date.now();
  const engine = new ArcadeEngine("data/arcade.sqlite")
  const phantom = new PhantomApp("nexus-arcade");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  if (engine.listGames().length === 0) {
    engine.addGame({ title: "Super Mario Bros", platform: "NES", year: 1985, genre: "platform" });
    engine.addGame({ title: "Pac-Man", platform: "Arcade", year: 1980, genre: "arcade" });
  }

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-arcade", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/arcade/games") {
        const platform = url.searchParams.get("platform") || undefined;
        const genre = url.searchParams.get("genre") || undefined;
        return json({ games: engine.listGames({ platform, genre }), mostPlayed: engine.getMostPlayed(5) });
      }
      if (req.method === "POST" && p === "/api/v1/arcade/games") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.title || !b.platform) return json({ error: "title and platform required" }, 400);
        return json(engine.addGame({ title: b.title as string, platform: b.platform as string, year: b.year as number, genre: b.genre as string, romHash: b.romHash as string }), 201);
      }
      const gameMatch = p.match(/^\/api\/v1\/arcade\/games\/([^/]+)$/);
      if (req.method === "GET" && gameMatch) {
        const g = engine.getGame(gameMatch[1]!);
        return g ? json({ game: g, leaderboard: engine.getLeaderboard(gameMatch[1]!) }) : json({ error: "not found" }, 404);
      }
      if (req.method === "POST" && p === "/api/v1/arcade/sessions") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.gameId || !b.userId) return json({ error: "gameId and userId required" }, 400);
        return json(engine.startSession(b.gameId as string, b.userId as string), 201);
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-arcade] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
