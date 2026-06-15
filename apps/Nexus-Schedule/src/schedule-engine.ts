import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Schedule {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class ScheduleEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS schedules (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createSchedule(name: string, type = ""): Schedule {
    const d: Schedule = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO schedules VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listSchedules(): Schedule[] {
    return this.db.prepare("SELECT * FROM schedules ORDER BY updated_at DESC").all() as Schedule[];
  }

  getSchedule(id: string): Schedule | undefined {
    return this.db.prepare("SELECT * FROM schedules WHERE id = ?").get(id) as Schedule | undefined;
  }
}
