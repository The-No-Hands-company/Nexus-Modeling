import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface ContentItem { id: string; title: string; type: string; body: string; tags: string[]; status: string; createdAt: string; }
export class ContentEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS content (id TEXT PRIMARY KEY, title TEXT, type TEXT, body TEXT, tags TEXT DEFAULT '[]', status TEXT DEFAULT 'draft', created_at TEXT)`); }
  createContent(c: { title: string; type: string; body?: string; tags?: string[]; status?: string }): ContentItem {
    const item: ContentItem = { id: randomUUID(), title: c.title, type: c.type, body: c.body || "", tags: c.tags || [], status: c.status || "draft", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO content VALUES (?,?,?,?,?,?,?)").run(item.id, item.title, item.type, item.body, JSON.stringify(item.tags), item.status, item.createdAt);
    return item;
  }
  listContent(type?: string): ContentItem[] {
    const rows = type ? this.db.prepare("SELECT * FROM content WHERE type = ? ORDER BY created_at DESC").all(type) : this.db.prepare("SELECT * FROM content ORDER BY created_at DESC").all();
    return (rows as Array<Record<string,unknown>>).map((r: Record<string,unknown>) => ({ ...r, tags: JSON.parse(r.tags as string) } as ContentItem));
  }
  getContent(id: string): ContentItem | undefined { const r = this.db.prepare("SELECT * FROM content WHERE id = ?").get(id) as Record<string,unknown> | undefined; return r ? { ...r, tags: JSON.parse(r.tags as string) } as ContentItem : undefined; }
  publishContent(id: string): void { this.db.prepare("UPDATE content SET status = 'published' WHERE id = ?").run(id); }
}
