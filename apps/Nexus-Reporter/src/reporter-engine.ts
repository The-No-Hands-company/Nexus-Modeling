import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Reporter {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class ReporterEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS reporters (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createReporter(name: string, type = ""): Reporter {
    const d: Reporter = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO reporters VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listReporters(): Reporter[] {
    return this.db.prepare("SELECT * FROM reporters ORDER BY updated_at DESC").all() as Reporter[];
  }

  getReporter(id: string): Reporter | undefined {
    return this.db.prepare("SELECT * FROM reporters WHERE id = ?").get(id) as Reporter | undefined;
  }
}
