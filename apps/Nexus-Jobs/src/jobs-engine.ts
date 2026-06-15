import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Jobs {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class JobsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS jobses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createJobs(name: string, type = ""): Jobs {
    const d: Jobs = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO jobses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listJobss(): Jobs[] {
    return this.db.prepare("SELECT * FROM jobses ORDER BY updated_at DESC").all() as Jobs[];
  }

  getJobs(id: string): Jobs | undefined {
    return this.db.prepare("SELECT * FROM jobses WHERE id = ?").get(id) as Jobs | undefined;
  }
}
