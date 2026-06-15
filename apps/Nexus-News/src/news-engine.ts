import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface News {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class NewsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS newses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createNews(name: string, type = ""): News {
    const d: News = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO newses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listNewss(): News[] {
    return this.db.prepare("SELECT * FROM newses ORDER BY updated_at DESC").all() as News[];
  }

  getNews(id: string): News | undefined {
    return this.db.prepare("SELECT * FROM newses WHERE id = ?").get(id) as News | undefined;
  }
}
