import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Presence { id: string; userId: string; status: string; location: string; device: string; lastSeen: string; createdAt: string; }
export interface Location { id: string; name: string; type: string; lat: number; lng: number; createdAt: string; }
export class PresenceEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS presence (id TEXT PRIMARY KEY, user_id TEXT, status TEXT DEFAULT 'offline', location TEXT, device TEXT, last_seen TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS locations (id TEXT PRIMARY KEY, name TEXT, loc_type TEXT, lat REAL, lng REAL, created_at TEXT)"); }
  setPresence(userId: string, status: string, location = "", device = ""): Presence {
    const p: Presence = { id: randomUUID(), userId, status, location, device, lastSeen: new Date().toISOString(), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO presence VALUES (?,?,?,?,?,?,?)").run(p.id, p.userId, p.status, p.location, p.device, p.lastSeen, p.createdAt); return p; }
  getPresence(userId: string): Presence | undefined { return this.db.prepare("SELECT * FROM presence WHERE user_id = ? ORDER BY last_seen DESC").get(userId) as Presence | undefined; }
  listActive(status?: string): Presence[] {
    if (status) return this.db.prepare("SELECT * FROM presence WHERE status = ? ORDER BY last_seen DESC").all(status) as Presence[];
    return this.db.prepare("SELECT * FROM presence ORDER BY last_seen DESC").all() as Presence[]; }
  addLocation(name: string, type: string, lat: number, lng: number): Location {
    const l: Location = { id: randomUUID(), name, type, lat, lng, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO locations VALUES (?,?,?,?,?,?)").run(l.id, l.name, l.type, l.lat, l.lng, l.createdAt); return l; }
  listLocations(): Location[] { return this.db.prepare("SELECT * FROM locations ORDER BY name ASC").all() as Location[]; } }
