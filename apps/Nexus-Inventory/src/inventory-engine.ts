import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Inventory {
  id: string;
  name: string;
  description: string;
  sku: string;
  createdAt: string;
  updatedAt: string;
}

export class InventoryEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS inventories (id TEXT PRIMARY KEY, name TEXT, description TEXT, sku TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createInventory(name: string, sku = ""): Inventory {
    const d: Inventory = {
      id: randomUUID(),
      name,
      description: "",
      sku: sku,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO inventories VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.sku, d.createdAt, d.updatedAt);
    return d;
  }

  listInventorys(): Inventory[] {
    return this.db.prepare("SELECT * FROM inventories ORDER BY updated_at DESC").all() as Inventory[];
  }

  getInventory(id: string): Inventory | undefined {
    return this.db.prepare("SELECT * FROM inventories WHERE id = ?").get(id) as Inventory | undefined;
  }
}
