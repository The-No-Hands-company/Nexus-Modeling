import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Task { id: string; title: string; description: string; listId: string; assignee: string; priority: string; dueDate: string; completed: number; createdAt: string; }
export interface TaskList { id: string; name: string; description: string; taskCount: number; createdAt: string; }
export class TasksEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY, title TEXT, description TEXT, list_id TEXT, assignee TEXT, priority TEXT DEFAULT 'medium', due_date TEXT, completed INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS task_lists (id TEXT PRIMARY KEY, name TEXT, description TEXT, task_count INTEGER DEFAULT 0, created_at TEXT)"); }
  addTask(title: string, description: string, listId: string, assignee = "", priority = "medium", dueDate = ""): Task {
    const t: Task = { id: randomUUID(), title, description, listId, assignee, priority, dueDate, completed: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO tasks VALUES (?,?,?,?,?,?,?,?,?)").run(t.id, t.title, t.description, t.listId, t.assignee, t.priority, t.dueDate, t.completed, t.createdAt);
    this.db.prepare("UPDATE task_lists SET task_count = task_count + 1 WHERE id = ?").run(listId); return t; }
  listTasks(listId?: string, priority?: string): Task[] {
    let q = "SELECT * FROM tasks WHERE 1=1", p: any[] = [];
    if (listId) { q += " AND list_id = ?"; p.push(listId); }
    if (priority) { q += " AND priority = ?"; p.push(priority); }
    q += " ORDER BY completed ASC, created_at DESC"; return this.db.prepare(q).all(...p) as Task[]; }
  getTask(id: string): Task | undefined { return this.db.prepare("SELECT * FROM tasks WHERE id = ?").get(id) as Task | undefined; }
  completeTask(id: string): void { this.db.prepare("UPDATE tasks SET completed = 1 WHERE id = ?").run(id); }
  addList(name: string, description = ""): TaskList {
    const l: TaskList = { id: randomUUID(), name, description, taskCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO task_lists VALUES (?,?,?,?,?)").run(l.id, l.name, l.description, l.taskCount, l.createdAt); return l; }
  listLists(): TaskList[] { return this.db.prepare("SELECT * FROM task_lists ORDER BY name ASC").all() as TaskList[]; } }
