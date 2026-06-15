import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Logistics {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class LogisticsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS logisticses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createLogistics(name: string, type = ""): Logistics {
    const d: Logistics = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO logisticses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listLogisticss(): Logistics[] {
    return this.db.prepare("SELECT * FROM logisticses ORDER BY updated_at DESC").all() as Logistics[];
  }

  getLogistics(id: string): Logistics | undefined {
    return this.db.prepare("SELECT * FROM logisticses WHERE id = ?").get(id) as Logistics | undefined;
  }
}
