import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Room { id: string; name: string; devices: number; createdAt: string; updatedAt: string; }
export interface Device { id: string; roomId: string; name: string; type: string; status: "on" | "off"; value: string; createdAt: string; }
export class HomeEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS rooms (id TEXT PRIMARY KEY, name TEXT, devices INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS devices (id TEXT PRIMARY KEY, room_id TEXT, name TEXT, type TEXT, status TEXT DEFAULT 'off', value TEXT DEFAULT '', created_at TEXT)");
  }
  createRoom(name: string): Room {
    const r: Room = { id: randomUUID(), name, devices: 0, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO rooms VALUES (?,?,?,?,?)").run(r.id, r.name, r.devices, r.createdAt, r.updatedAt); return r;
  }
  listRooms(): Room[] { return this.db.prepare("SELECT * FROM rooms ORDER BY name ASC").all() as Room[]; }
  addDevice(roomId: string, name: string, type: string): Device {
    const d: Device = { id: randomUUID(), roomId, name, type, status: "off", value: "", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO devices VALUES (?,?,?,?,?,?,?)").run(d.id, d.roomId, d.name, d.type, d.status, d.value, d.createdAt);
    this.db.prepare("UPDATE rooms SET devices = devices + 1, updated_at = ? WHERE id = ?").run(d.createdAt, roomId); return d;
  }
  listDevices(roomId?: string): Device[] {
    if (roomId) return this.db.prepare("SELECT * FROM devices WHERE room_id = ? ORDER BY name ASC").all(roomId) as Device[];
    return this.db.prepare("SELECT * FROM devices ORDER BY name ASC").all() as Device[];
  }
  toggleDevice(id: string): Device | undefined {
    const d = this.db.prepare("SELECT * FROM devices WHERE id = ?").get(id) as Device | undefined;
    if (!d) return undefined;
    const newStatus = d.status === "on" ? "off" : "on";
    this.db.prepare("UPDATE devices SET status = ? WHERE id = ?").run(newStatus, id);
    return { ...d, status: newStatus };
  }
}
