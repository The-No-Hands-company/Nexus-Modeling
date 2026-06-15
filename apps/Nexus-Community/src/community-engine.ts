import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Community {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class CommunityEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS communities (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createCommunity(name: string, type = ""): Community {
    const d: Community = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO communities VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listCommunitys(): Community[] {
    return this.db.prepare("SELECT * FROM communities ORDER BY updated_at DESC").all() as Community[];
  }

  getCommunity(id: string): Community | undefined {
    return this.db.prepare("SELECT * FROM communities WHERE id = ?").get(id) as Community | undefined;
  }
}
