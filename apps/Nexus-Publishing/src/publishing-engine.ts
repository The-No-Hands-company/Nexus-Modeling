import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Publishing {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class PublishingEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS publishings (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createPublishing(name: string, type = ""): Publishing {
    const d: Publishing = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO publishings VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listPublishings(): Publishing[] {
    return this.db.prepare("SELECT * FROM publishings ORDER BY updated_at DESC").all() as Publishing[];
  }

  getPublishing(id: string): Publishing | undefined {
    return this.db.prepare("SELECT * FROM publishings WHERE id = ?").get(id) as Publishing | undefined;
  }
}
