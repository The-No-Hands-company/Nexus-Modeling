import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Game { id: string; title: string; platform: string; year: number; genre: string; romHash: string | undefined; timesPlayed: number; rating: number; addedAt: string; }
export interface GameSession { id: string; gameId: string; userId: string; startedAt: string; endedAt: string | undefined; durationSecs: number | undefined; score: number | undefined; }

export class ArcadeEngine {
  db: Database;
  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec(`CREATE TABLE IF NOT EXISTS games (id TEXT PRIMARY KEY, title TEXT, platform TEXT, year INTEGER, genre TEXT, rom_hash TEXT, times_played INTEGER DEFAULT 0, rating REAL DEFAULT 0, added_at TEXT); CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, game_id TEXT, user_id TEXT, started_at TEXT, ended_at TEXT, duration_secs INTEGER, score INTEGER)`);
  }

  addGame(g: { title: string; platform: string; year?: number; genre?: string; romHash?: string }): Game {
    const game: Game = { id: randomUUID(), title: g.title, platform: g.platform, year: g.year || 0, genre: g.genre || "other", romHash: g.romHash, timesPlayed: 0, rating: 0, addedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO games (id, title, platform, year, genre, rom_hash, times_played, rating, added_at) VALUES (?,?,?,?,?,?,?,?,?)").run(game.id, game.title, game.platform, game.year, game.genre, game.romHash || null, game.timesPlayed, game.rating, game.addedAt);
    return game;
  }

  listGames(filter?: { platform?: string; genre?: string }): Game[] {
    let q = "SELECT * FROM games WHERE 1=1"; const p: unknown[] = [];
    if (filter?.platform) { q += " AND platform = ?"; p.push(filter.platform); }
    if (filter?.genre) { q += " AND genre = ?"; p.push(filter.genre); }
    return this.db.prepare(q + " ORDER BY times_played DESC, rating DESC").all(...p as any) as Game[];
  }

  getGame(id: string): Game | undefined { return this.db.prepare("SELECT * FROM games WHERE id = ?").get(id) as Game | undefined; }

  rateGame(id: string, rating: number): void {
    this.db.prepare("UPDATE games SET rating = ? WHERE id = ?").run(Math.min(5, Math.max(0, rating)), id);
  }

  startSession(gameId: string, userId: string): GameSession {
    const s: GameSession = { id: randomUUID(), gameId, userId, startedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO sessions (id, game_id, user_id, started_at) VALUES (?,?,?,?)").run(s.id, s.gameId, s.userId, s.startedAt);
    this.db.prepare("UPDATE games SET times_played = times_played + 1 WHERE id = ?").run(gameId);
    return s;
  }

  endSession(sessionId: string, score?: number): GameSession | undefined {
    const s = this.db.prepare("SELECT * FROM sessions WHERE id = ?").get(sessionId) as GameSession | undefined;
    if (!s) return undefined;
    const now = new Date().toISOString();
    const durationSecs = Math.floor((Date.now() - new Date(s.startedAt).getTime()) / 1000);
    this.db.prepare("UPDATE sessions SET ended_at = ?, duration_secs = ?, score = ? WHERE id = ?").run(now, durationSecs, score || 0, sessionId);
    return { ...s, endedAt: now, durationSecs, score };
  }

  getLeaderboard(gameId: string, limit = 10): Array<{ userId: string; highScore: number }> {
    return this.db.prepare("SELECT user_id, MAX(score) as high_score FROM sessions WHERE game_id = ? AND score > 0 GROUP BY user_id ORDER BY high_score DESC LIMIT ?").all(gameId, limit) as Array<{ userId: string; highScore: number }>;
  }

  getMostPlayed(limit = 10): Game[] {
    return this.db.prepare("SELECT * FROM games ORDER BY times_played DESC LIMIT ?").all(limit) as Game[];
  }
}
