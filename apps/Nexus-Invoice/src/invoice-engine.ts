import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Invoice {
  id: string;
  name: string;
  description: string;
  amount: string;
  createdAt: string;
  updatedAt: string;
}

export class InvoiceEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS invoices (id TEXT PRIMARY KEY, name TEXT, description TEXT, amount TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createInvoice(name: string, amount = ""): Invoice {
    const d: Invoice = {
      id: randomUUID(),
      name,
      description: "",
      amount: amount,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO invoices VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.amount, d.createdAt, d.updatedAt);
    return d;
  }

  listInvoices(): Invoice[] {
    return this.db.prepare("SELECT * FROM invoices ORDER BY updated_at DESC").all() as Invoice[];
  }

  getInvoice(id: string): Invoice | undefined {
    return this.db.prepare("SELECT * FROM invoices WHERE id = ?").get(id) as Invoice | undefined;
  }
}
