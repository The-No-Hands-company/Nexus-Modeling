import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Journal {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class JournalEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS journals (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createJournal(name: string, type = ""): Journal {
    const d: Journal = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO journals VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listJournals(): Journal[] {
    return this.db.prepare("SELECT * FROM journals ORDER BY updated_at DESC").all() as Journal[];
  }

  getJournal(id: string): Journal | undefined {
    return this.db.prepare("SELECT * FROM journals WHERE id = ?").get(id) as Journal | undefined;
  }
}
