import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Docs {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class DocsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS docses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createDocs(name: string, type = ""): Docs {
    const d: Docs = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO docses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listDocss(): Docs[] {
    return this.db.prepare("SELECT * FROM docses ORDER BY updated_at DESC").all() as Docs[];
  }

  getDocs(id: string): Docs | undefined {
    return this.db.prepare("SELECT * FROM docses WHERE id = ?").get(id) as Docs | undefined;
  }
}
