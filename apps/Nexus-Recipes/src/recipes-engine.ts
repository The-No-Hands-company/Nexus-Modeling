import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Recipes {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class RecipesEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS recipeses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createRecipes(name: string, type = ""): Recipes {
    const d: Recipes = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO recipeses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listRecipess(): Recipes[] {
    return this.db.prepare("SELECT * FROM recipeses ORDER BY updated_at DESC").all() as Recipes[];
  }

  getRecipes(id: string): Recipes | undefined {
    return this.db.prepare("SELECT * FROM recipeses WHERE id = ?").get(id) as Recipes | undefined;
  }
}
