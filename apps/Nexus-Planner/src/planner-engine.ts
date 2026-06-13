import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Plan { id: string; title: string; description: string; startDate: string; endDate: string; status: string; createdAt: string; }
export interface PlanTask { id: string; planId: string; title: string; completed: number; dueDate: string; createdAt: string; }
export class PlannerEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS plans (id TEXT PRIMARY KEY, title TEXT, description TEXT, start_date TEXT, end_date TEXT, status TEXT DEFAULT 'active', created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS plan_tasks (id TEXT PRIMARY KEY, plan_id TEXT, title TEXT, completed INTEGER DEFAULT 0, due_date TEXT, created_at TEXT)"); }
  addPlan(title: string, description: string, startDate: string, endDate: string): Plan {
    const p: Plan = { id: randomUUID(), title, description, startDate, endDate, status: "active", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO plans VALUES (?,?,?,?,?,?,?)").run(p.id, p.title, p.description, p.startDate, p.endDate, p.status, p.createdAt); return p; }
  listPlans(status?: string): Plan[] {
    if (status) return this.db.prepare("SELECT * FROM plans WHERE status = ? ORDER BY start_date ASC").all(status) as Plan[];
    return this.db.prepare("SELECT * FROM plans ORDER BY start_date DESC").all() as Plan[]; }
  getPlan(id: string): Plan | undefined { return this.db.prepare("SELECT * FROM plans WHERE id = ?").get(id) as Plan | undefined; }
  addPlanTask(planId: string, title: string, dueDate = ""): PlanTask {
    const pt: PlanTask = { id: randomUUID(), planId, title, completed: 0, dueDate, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO plan_tasks VALUES (?,?,?,?,?,?)").run(pt.id, pt.planId, pt.title, pt.completed, pt.dueDate, pt.createdAt); return pt; }
  listPlanTasks(planId: string): PlanTask[] { return this.db.prepare("SELECT * FROM plan_tasks WHERE plan_id = ? ORDER BY created_at ASC").all(planId) as PlanTask[]; }
  completePlanTask(id: string): void { this.db.prepare("UPDATE plan_tasks SET completed = 1 WHERE id = ?").run(id); } }
