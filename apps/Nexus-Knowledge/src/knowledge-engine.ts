import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Knowledge {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class KnowledgeEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS knowledges (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createKnowledge(name: string, type = ""): Knowledge {
    const d: Knowledge = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO knowledges VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listKnowledges(): Knowledge[] {
    return this.db.prepare("SELECT * FROM knowledges ORDER BY updated_at DESC").all() as Knowledge[];
  }

  getKnowledge(id: string): Knowledge | undefined {
    return this.db.prepare("SELECT * FROM knowledges WHERE id = ?").get(id) as Knowledge | undefined;
  }
}
