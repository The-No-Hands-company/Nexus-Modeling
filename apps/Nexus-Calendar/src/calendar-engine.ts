import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface CalEvent { id: string; title: string; description: string | undefined; location: string | undefined; startTime: string; endTime: string; allDay: boolean; recurrence: string | undefined; createdAt: string; }
export class CalendarEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS events (id TEXT PRIMARY KEY, title TEXT, description TEXT, location TEXT, start_time TEXT, end_time TEXT, all_day INTEGER DEFAULT 0, recurrence TEXT, created_at TEXT)`); }
  createEvent(e: { title: string; description: string | undefined; location: string | undefined; startTime: string; endTime: string; allDay?: boolean; recurrence: string | undefined }): CalEvent {
    const ev: CalEvent = { id: randomUUID(), title: e.title, description: e.description, location: e.location, startTime: e.startTime, endTime: e.endTime, allDay: e.allDay || false, recurrence: e.recurrence, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO events VALUES (?,?,?,?,?,?,?,?,?)").run(ev.id, ev.title, ev.description||null, ev.location||null, ev.startTime, ev.endTime, ev.allDay?1:0, ev.recurrence||null, ev.createdAt);
    return ev;
  }
  listEvents(from: string, to: string): CalEvent[] { return this.db.prepare("SELECT * FROM events WHERE start_time >= ? AND end_time <= ? ORDER BY start_time").all(from, to) as CalEvent[]; }
  deleteEvent(id: string): boolean { return this.db.prepare("DELETE FROM events WHERE id = ?").run(id).changes > 0; }
}
