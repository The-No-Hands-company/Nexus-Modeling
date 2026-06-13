import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Game { id: string; name: string; genre: string; maxPlayers: number; createdAt: string; }
export interface GameSession { id: string; gameId: string; players: string; score: number; duration: number; status: string; startedAt: string; endedAt: string; createdAt: string; }
export class PlayEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS games (id TEXT PRIMARY KEY, name TEXT, genre TEXT, max_players INTEGER DEFAULT 4, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS game_sessions (id TEXT PRIMARY KEY, game_id TEXT, players TEXT, score INTEGER DEFAULT 0, duration REAL, status TEXT DEFAULT 'active', started_at TEXT, ended_at TEXT, created_at TEXT)"); }
  addGame(name: string, genre: string, maxPlayers = 4): Game {
    const g: Game = { id: randomUUID(), name, genre, maxPlayers, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO games VALUES (?,?,?,?,?)").run(g.id, g.name, g.genre, g.maxPlayers, g.createdAt); return g; }
  listGames(genre?: string): Game[] {
    if (genre) return this.db.prepare("SELECT * FROM games WHERE genre = ? ORDER BY name ASC").all(genre) as Game[];
    return this.db.prepare("SELECT * FROM games ORDER BY name ASC").all() as Game[]; }
  getGame(id: string): Game | undefined { return this.db.prepare("SELECT * FROM games WHERE id = ?").get(id) as Game | undefined; }
  startSession(gameId: string, players: string): GameSession {
    const s: GameSession = { id: randomUUID(), gameId, players, score: 0, duration: 0, status: "active", startedAt: new Date().toISOString(), endedAt: "", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO game_sessions VALUES (?,?,?,?,?,?,?,?,?)").run(s.id, s.gameId, s.players, s.score, s.duration, s.status, s.startedAt, s.endedAt, s.createdAt); return s; }
  listSessions(gameId?: string, status?: string): GameSession[] {
    let q = "SELECT * FROM game_sessions WHERE 1=1", p: any[] = [];
    if (gameId) { q += " AND game_id = ?"; p.push(gameId); }
    if (status) { q += " AND status = ?"; p.push(status); }
    q += " ORDER BY created_at DESC";
    return this.db.prepare(q).all(...p) as GameSession[]; }
  endSession(id: string, score: number, duration: number): void {
    this.db.prepare("UPDATE game_sessions SET status = 'ended', score = ?, duration = ?, ended_at = ? WHERE id = ?").run(score, duration, new Date().toISOString(), id); } }
