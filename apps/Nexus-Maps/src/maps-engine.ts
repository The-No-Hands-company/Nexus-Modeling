import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Maps {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class MapsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS mapses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createMaps(name: string, type = ""): Maps {
    const d: Maps = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO mapses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listMapss(): Maps[] {
    return this.db.prepare("SELECT * FROM mapses ORDER BY updated_at DESC").all() as Maps[];
  }

  getMaps(id: string): Maps | undefined {
    return this.db.prepare("SELECT * FROM mapses WHERE id = ?").get(id) as Maps | undefined;
  }
}
