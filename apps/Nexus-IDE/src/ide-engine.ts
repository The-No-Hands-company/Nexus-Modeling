import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface IDEProject { id: string; name: string; language: string; files: number; createdAt: string; updatedAt: string; }
export interface IDEFile { id: string; projectId: string; name: string; content: string; language: string; createdAt: string; updatedAt: string; }
export class IDEEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS projects (id TEXT PRIMARY KEY, name TEXT, language TEXT, files INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS files (id TEXT PRIMARY KEY, project_id TEXT, name TEXT, content TEXT, language TEXT, created_at TEXT, updated_at TEXT)");
  }
  createProject(name: string, language: string): IDEProject {
    const p: IDEProject = { id: randomUUID(), name, language, files: 0, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO projects VALUES (?,?,?,?,?,?)").run(p.id, p.name, p.language, p.files, p.createdAt, p.updatedAt); return p;
  }
  listProjects(): IDEProject[] { return this.db.prepare("SELECT * FROM projects ORDER BY updated_at DESC").all() as IDEProject[]; }
  getProject(id: string): IDEProject | undefined { return this.db.prepare("SELECT * FROM projects WHERE id = ?").get(id) as IDEProject | undefined; }
  addFile(projectId: string, name: string, content = "", language = ""): IDEFile {
    const f: IDEFile = { id: randomUUID(), projectId, name, content, language, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO files VALUES (?,?,?,?,?,?,?)").run(f.id, f.projectId, f.name, f.content, f.language, f.createdAt, f.updatedAt);
    this.db.prepare("UPDATE projects SET files = files + 1, updated_at = ? WHERE id = ?").run(f.createdAt, projectId); return f;
  }
  listFiles(projectId: string): IDEFile[] {
    return this.db.prepare("SELECT * FROM files WHERE project_id = ? ORDER BY name ASC").all(projectId) as IDEFile[];
  }
  updateFile(id: string, content: string): IDEFile | undefined {
    this.db.prepare("UPDATE files SET content = ?, updated_at = ? WHERE id = ?").run(content, new Date().toISOString(), id);
    return this.db.prepare("SELECT * FROM files WHERE id = ?").get(id) as IDEFile | undefined;
  }
}
