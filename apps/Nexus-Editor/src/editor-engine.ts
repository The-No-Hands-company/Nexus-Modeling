import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Editor {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class EditorEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS editors (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createEditor(name: string, type = ""): Editor {
    const d: Editor = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO editors VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listEditors(): Editor[] {
    return this.db.prepare("SELECT * FROM editors ORDER BY updated_at DESC").all() as Editor[];
  }

  getEditor(id: string): Editor | undefined {
    return this.db.prepare("SELECT * FROM editors WHERE id = ?").get(id) as Editor | undefined;
  }
}
