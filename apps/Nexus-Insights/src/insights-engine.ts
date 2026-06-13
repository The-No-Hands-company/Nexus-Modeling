import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Report { id: string; title: string; type: string; dataset: string; config: string; createdAt: string; }
export interface Dataset { id: string; name: string; rows: number; columns: string; createdAt: string; }
export class InsightsEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS reports (id TEXT PRIMARY KEY, title TEXT, type TEXT, dataset TEXT, config TEXT DEFAULT '{}', created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS datasets (id TEXT PRIMARY KEY, name TEXT, rows INTEGER DEFAULT 0, columns TEXT DEFAULT '[]', created_at TEXT)");
  }
  createDataset(name: string, columns: string[]): Dataset {
    const d: Dataset = { id: randomUUID(), name, rows: 0, columns: JSON.stringify(columns), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO datasets VALUES (?,?,?,?,?)").run(d.id, d.name, d.rows, d.columns, d.createdAt); return d;
  }
  listDatasets(): Dataset[] { return this.db.prepare("SELECT * FROM datasets ORDER BY created_at DESC").all() as Dataset[]; }
  createReport(title: string, type: string, dataset: string, config: Record<string, unknown> = {}): Report {
    const r: Report = { id: randomUUID(), title, type, dataset, config: JSON.stringify(config), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO reports VALUES (?,?,?,?,?,?)").run(r.id, r.title, r.type, r.dataset, r.config, r.createdAt); return r;
  }
  listReports(): Report[] { return this.db.prepare("SELECT * FROM reports ORDER BY created_at DESC").all() as Report[]; }
  getReport(id: string): Report | undefined { return this.db.prepare("SELECT * FROM reports WHERE id = ?").get(id) as Report | undefined; }
}
