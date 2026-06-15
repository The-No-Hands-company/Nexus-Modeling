import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Remote {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class RemoteEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS remotes (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createRemote(name: string, type = ""): Remote {
    const d: Remote = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO remotes VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listRemotes(): Remote[] {
    return this.db.prepare("SELECT * FROM remotes ORDER BY updated_at DESC").all() as Remote[];
  }

  getRemote(id: string): Remote | undefined {
    return this.db.prepare("SELECT * FROM remotes WHERE id = ?").get(id) as Remote | undefined;
  }
}
