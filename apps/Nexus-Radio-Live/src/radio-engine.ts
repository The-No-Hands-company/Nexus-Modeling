import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Station { id: string; name: string; frequency: string; genre: string; streamUrl: string; active: number; createdAt: string; }
export interface Program { id: string; stationId: string; name: string; description: string; startTime: string; endTime: string; dayOfWeek: number; createdAt: string; }
export class RadioEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS stations (id TEXT PRIMARY KEY, name TEXT, frequency TEXT, genre TEXT, stream_url TEXT, active INTEGER DEFAULT 1, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS programs (id TEXT PRIMARY KEY, station_id TEXT, name TEXT, description TEXT, start_time TEXT, end_time TEXT, day_of_week INTEGER, created_at TEXT)"); }
  addStation(name: string, frequency: string, genre: string, streamUrl: string): Station {
    const s: Station = { id: randomUUID(), name, frequency, genre, streamUrl, active: 1, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO stations VALUES (?,?,?,?,?,?,?)").run(s.id, s.name, s.frequency, s.genre, s.streamUrl, s.active, s.createdAt); return s; }
  listStations(genre?: string): Station[] {
    if (genre) return this.db.prepare("SELECT * FROM stations WHERE genre = ? ORDER BY name ASC").all(genre) as Station[];
    return this.db.prepare("SELECT * FROM stations ORDER BY name ASC").all() as Station[]; }
  getStation(id: string): Station | undefined { return this.db.prepare("SELECT * FROM stations WHERE id = ?").get(id) as Station | undefined; }
  addProgram(stationId: string, name: string, description: string, startTime: string, endTime: string, dayOfWeek: number): Program {
    const pr: Program = { id: randomUUID(), stationId, name, description, startTime, endTime, dayOfWeek, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO programs VALUES (?,?,?,?,?,?,?,?)").run(pr.id, pr.stationId, pr.name, pr.description, pr.startTime, pr.endTime, pr.dayOfWeek, pr.createdAt); return pr; }
  listPrograms(stationId: string): Program[] { return this.db.prepare("SELECT * FROM programs WHERE station_id = ? ORDER BY day_of_week ASC, start_time ASC").all(stationId) as Program[]; } }
