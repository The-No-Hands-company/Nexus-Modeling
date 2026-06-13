import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface HealthRecord { id: string; metric: string; value: number; unit: string; notes: string; recordedAt: string; createdAt: string; }
export interface HealthGoal { id: string; metric: string; target: number; current: number; unit: string; deadline: string; createdAt: string; }
export class HealthEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS records (id TEXT PRIMARY KEY, metric TEXT, value REAL, unit TEXT, notes TEXT, recorded_at TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS goals (id TEXT PRIMARY KEY, metric TEXT, target REAL, current REAL, unit TEXT, deadline TEXT, created_at TEXT)");
  }
  createRecord(metric: string, value: number, unit: string, notes = "", recordedAt?: string): HealthRecord {
    const r: HealthRecord = { id: randomUUID(), metric, value, unit, notes, recordedAt: recordedAt || new Date().toISOString(), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO records VALUES (?,?,?,?,?,?,?)").run(r.id, r.metric, r.value, r.unit, r.notes, r.recordedAt, r.createdAt); return r;
  }
  listRecords(): HealthRecord[] { return this.db.prepare("SELECT * FROM records ORDER BY recorded_at DESC").all() as HealthRecord[]; }
  getRecord(id: string): HealthRecord | undefined { return this.db.prepare("SELECT * FROM records WHERE id = ?").get(id) as HealthRecord | undefined; }
  createGoal(metric: string, target: number, unit: string, deadline: string): HealthGoal {
    const g: HealthGoal = { id: randomUUID(), metric, target, current: 0, unit, deadline, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO goals VALUES (?,?,?,?,?,?,?)").run(g.id, g.metric, g.target, g.current, g.unit, g.deadline, g.createdAt); return g;
  }
  listGoals(): HealthGoal[] { return this.db.prepare("SELECT * FROM goals ORDER BY deadline ASC").all() as HealthGoal[]; }
}
