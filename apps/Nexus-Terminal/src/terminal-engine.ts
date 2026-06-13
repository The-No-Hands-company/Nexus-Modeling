import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Session { id: string; userId: string; type: string; status: string; startedAt: string; endedAt: string; createdAt: string; }
export interface Command { id: string; sessionId: string; command: string; output: string; exitCode: number; executedAt: string; }
export class TerminalEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, user_id TEXT, session_type TEXT, status TEXT DEFAULT 'active', started_at TEXT, ended_at TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS commands (id TEXT PRIMARY KEY, session_id TEXT, command TEXT, output TEXT, exit_code INTEGER DEFAULT 0, executed_at TEXT)"); }
  createSession(userId: string, type: string): Session {
    const s: Session = { id: randomUUID(), userId, type, status: "active", startedAt: new Date().toISOString(), endedAt: "", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO sessions VALUES (?,?,?,?,?,?,?)").run(s.id, s.userId, s.type, s.status, s.startedAt, s.endedAt, s.createdAt); return s; }
  listSessions(userId?: string): Session[] {
    if (userId) return this.db.prepare("SELECT * FROM sessions WHERE user_id = ? ORDER BY started_at DESC").all(userId) as Session[];
    return this.db.prepare("SELECT * FROM sessions ORDER BY started_at DESC").all() as Session[]; }
  endSession(id: string): void { this.db.prepare("UPDATE sessions SET status = 'ended', ended_at = ? WHERE id = ?").run(new Date().toISOString(), id); }
  logCommand(sessionId: string, command: string, output: string, exitCode = 0): Command {
    const c: Command = { id: randomUUID(), sessionId, command, output, exitCode, executedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO commands VALUES (?,?,?,?,?,?)").run(c.id, c.sessionId, c.command, c.output, c.exitCode, c.executedAt); return c; }
  getSessionHistory(sessionId: string): Command[] { return this.db.prepare("SELECT * FROM commands WHERE session_id = ? ORDER BY executed_at ASC").all(sessionId) as Command[]; } }
