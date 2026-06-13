import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface MeetingRoom { id: string; name: string; participants: number; maxParticipants: number; status: "idle" | "active" | "ended"; createdAt: string; }
export interface Participant { id: string; roomId: string; name: string; joinedAt: string; }
export interface Schedule { id: string; title: string; roomId: string; startTime: string; duration: number; createdAt: string; }
export class MeetEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS rooms (id TEXT PRIMARY KEY, name TEXT, participants INTEGER DEFAULT 0, max_participants INTEGER DEFAULT 10, status TEXT DEFAULT 'idle', created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS participants (id TEXT PRIMARY KEY, room_id TEXT, name TEXT, joined_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS schedules (id TEXT PRIMARY KEY, title TEXT, room_id TEXT, start_time TEXT, duration INTEGER DEFAULT 30, created_at TEXT)");
  }
  createRoom(name: string, maxParticipants = 10): MeetingRoom {
    const r: MeetingRoom = { id: randomUUID(), name, participants: 0, maxParticipants, status: "idle", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO rooms VALUES (?,?,?,?,?,?)").run(r.id, r.name, r.participants, r.maxParticipants, r.status, r.createdAt); return r;
  }
  listRooms(): MeetingRoom[] { return this.db.prepare("SELECT * FROM rooms ORDER BY created_at DESC").all() as MeetingRoom[]; }
  joinRoom(roomId: string, name: string): Participant | undefined {
    const room = this.db.prepare("SELECT * FROM rooms WHERE id = ?").get(roomId) as MeetingRoom | undefined;
    if (!room || room.participants >= room.maxParticipants) return undefined;
    const p: Participant = { id: randomUUID(), roomId, name, joinedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO participants VALUES (?,?,?,?)").run(p.id, p.roomId, p.name, p.joinedAt);
    this.db.prepare("UPDATE rooms SET participants = participants + 1, status = 'active' WHERE id = ?").run(roomId); return p;
  }
  listParticipants(roomId: string): Participant[] {
    return this.db.prepare("SELECT * FROM participants WHERE room_id = ? ORDER BY joined_at ASC").all(roomId) as Participant[];
  }
  scheduleMeeting(title: string, roomId: string, startTime: string, duration = 30): Schedule {
    const s: Schedule = { id: randomUUID(), title, roomId, startTime, duration, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO schedules VALUES (?,?,?,?,?,?)").run(s.id, s.title, s.roomId, s.startTime, s.duration, s.createdAt); return s;
  }
  listSchedules(): Schedule[] { return this.db.prepare("SELECT * FROM schedules ORDER BY start_time ASC").all() as Schedule[]; }
}
