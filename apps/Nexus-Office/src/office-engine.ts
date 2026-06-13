import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Document { id: string; title: string; content: string; type: string; tags: string; version: number; createdAt: string; updatedAt: string; }
export interface Template { id: string; name: string; content: string; category: string; createdAt: string; }
export class OfficeEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS documents (id TEXT PRIMARY KEY, title TEXT, content TEXT, doc_type TEXT, tags TEXT, version INTEGER DEFAULT 1, created_at TEXT, updated_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS templates (id TEXT PRIMARY KEY, name TEXT, content TEXT, category TEXT, created_at TEXT)"); }
  addDocument(title: string, content: string, type: string, tags = ""): Document {
    const d: Document = { id: randomUUID(), title, content, type, tags, version: 1, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO documents VALUES (?,?,?,?,?,?,?,?)").run(d.id, d.title, d.content, d.type, d.tags, d.version, d.createdAt, d.updatedAt); return d; }
  listDocuments(type?: string): Document[] {
    if (type) return this.db.prepare("SELECT * FROM documents WHERE doc_type = ? ORDER BY updated_at DESC").all(type) as Document[];
    return this.db.prepare("SELECT * FROM documents ORDER BY updated_at DESC").all() as Document[]; }
  getDocument(id: string): Document | undefined { return this.db.prepare("SELECT * FROM documents WHERE id = ?").get(id) as Document | undefined; }
  addTemplate(name: string, content: string, category: string): Template {
    const t: Template = { id: randomUUID(), name, content, category, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO templates VALUES (?,?,?,?,?)").run(t.id, t.name, t.content, t.category, t.createdAt); return t; }
  listTemplates(category?: string): Template[] {
    if (category) return this.db.prepare("SELECT * FROM templates WHERE category = ? ORDER BY name ASC").all(category) as Template[];
    return this.db.prepare("SELECT * FROM templates ORDER BY name ASC").all() as Template[]; } }
