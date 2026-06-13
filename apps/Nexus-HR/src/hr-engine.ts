import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Employee { id: string; name: string; email: string; department: string; position: string; salary: number; hiredAt: string; createdAt: string; }
export interface Department { id: string; name: string; headCount: number; createdAt: string; }
export class HREngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS employees (id TEXT PRIMARY KEY, name TEXT, email TEXT, department TEXT, position TEXT, salary REAL, hired_at TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS departments (id TEXT PRIMARY KEY, name TEXT, head_count INTEGER DEFAULT 0, created_at TEXT)");
  }
  createDepartment(name: string): Department {
    const d: Department = { id: randomUUID(), name, headCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO departments VALUES (?,?,?,?)").run(d.id, d.name, d.headCount, d.createdAt); return d;
  }
  listDepartments(): Department[] { return this.db.prepare("SELECT * FROM departments ORDER BY name ASC").all() as Department[]; }
  createEmployee(name: string, email: string, department: string, position: string, salary: number): Employee {
    const e: Employee = { id: randomUUID(), name, email, department, position, salary, hiredAt: new Date().toISOString(), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO employees VALUES (?,?,?,?,?,?,?,?)").run(e.id, e.name, e.email, e.department, e.position, e.salary, e.hiredAt, e.createdAt);
    this.db.prepare("UPDATE departments SET head_count = head_count + 1 WHERE name = ?").run(department); return e;
  }
  listEmployees(department?: string): Employee[] {
    if (department) return this.db.prepare("SELECT * FROM employees WHERE department = ? ORDER BY name ASC").all(department) as Employee[];
    return this.db.prepare("SELECT * FROM employees ORDER BY name ASC").all() as Employee[];
  }
  getEmployee(id: string): Employee | undefined { return this.db.prepare("SELECT * FROM employees WHERE id = ?").get(id) as Employee | undefined; }
}
