import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Expense { id: string; description: string; amount: number; category: string; date: string; vendor: string; createdAt: string; }
export interface Budget { id: string; name: string; category: string; amount: number; spent: number; period: string; createdAt: string; }
export class SpendEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS expenses (id TEXT PRIMARY KEY, description TEXT, amount REAL, category TEXT, date TEXT, vendor TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS budgets (id TEXT PRIMARY KEY, name TEXT, category TEXT, amount REAL, spent REAL DEFAULT 0, period TEXT, created_at TEXT)"); }
  addExpense(description: string, amount: number, category: string, date: string, vendor = ""): Expense {
    const e: Expense = { id: randomUUID(), description, amount, category, date, vendor, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO expenses VALUES (?,?,?,?,?,?,?)").run(e.id, e.description, e.amount, e.category, e.date, e.vendor, e.createdAt);
    this.db.prepare("UPDATE budgets SET spent = spent + ? WHERE category = ? AND period = ?").run(amount, category, date.substring(0, 7)); return e; }
  listExpenses(category?: string): Expense[] {
    if (category) return this.db.prepare("SELECT * FROM expenses WHERE category = ? ORDER BY date DESC").all(category) as Expense[];
    return this.db.prepare("SELECT * FROM expenses ORDER BY date DESC").all() as Expense[]; }
  getExpense(id: string): Expense | undefined { return this.db.prepare("SELECT * FROM expenses WHERE id = ?").get(id) as Expense | undefined; }
  addBudget(name: string, category: string, amount: number, period: string): Budget {
    const b: Budget = { id: randomUUID(), name, category, amount, spent: 0, period, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO budgets VALUES (?,?,?,?,?,?,?)").run(b.id, b.name, b.category, b.amount, b.spent, b.period, b.createdAt); return b; }
  listBudgets(): Budget[] { return this.db.prepare("SELECT * FROM budgets ORDER BY name ASC").all() as Budget[]; } }
