import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Commerce {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class CommerceEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS commerces (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createCommerce(name: string, type = ""): Commerce {
    const d: Commerce = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO commerces VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listCommerces(): Commerce[] {
    return this.db.prepare("SELECT * FROM commerces ORDER BY updated_at DESC").all() as Commerce[];
  }

  getCommerce(id: string): Commerce | undefined {
    return this.db.prepare("SELECT * FROM commerces WHERE id = ?").get(id) as Commerce | undefined;
  }
}
