import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface Ebook { id: string; title: string; author: string; format: string; tags: string[]; summary?: string; filePath?: string; readCount: number; addedAt: string; }
export class BookEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(`CREATE TABLE IF NOT EXISTS books (id TEXT PRIMARY KEY, title TEXT, author TEXT, format TEXT, tags TEXT DEFAULT '[]', summary TEXT, file_path TEXT, read_count INTEGER DEFAULT 0, added_at TEXT)`);
  }
  addBook(b: { title: string; author: string; format: string; tags?: string[] }): Ebook {
    const book: Ebook = { id: randomUUID(), title: b.title, author: b.author, format: b.format, tags: b.tags || [], readCount: 0, addedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO books VALUES (?,?,?,?,?,?,?,?,?)").run(book.id, book.title, book.author, book.format, JSON.stringify(book.tags), null, null, 0, book.addedAt);
    return book;
  }
  listBooks(query?: string): Ebook[] {
    const rows = query ? this.db.prepare("SELECT * FROM books WHERE title LIKE ? OR author LIKE ? ORDER BY added_at DESC").all(`%${query}%`, `%${query}%`) : this.db.prepare("SELECT * FROM books ORDER BY added_at DESC").all();
    return (rows as Array<Record<string,unknown>>).map((r: Record<string,unknown>) => ({ ...r, tags: JSON.parse(r.tags as string) } as Ebook));
  }
  getBook(id: string): Ebook | undefined {
    const r = this.db.prepare("SELECT * FROM books WHERE id = ?").get(id) as Record<string,unknown> | undefined;
    return r ? { ...r, tags: JSON.parse(r.tags as string) } as Ebook : undefined;
  }
  setSummary(id: string, summary: string): void { this.db.prepare("UPDATE books SET summary = ? WHERE id = ?").run(summary, id); }
  incrementRead(id: string): void { this.db.prepare("UPDATE books SET read_count = read_count + 1 WHERE id = ?").run(id); }
}
