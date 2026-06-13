import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Contact { id: string; name: string; email: string | undefined; phone: string | undefined; company: string | undefined; tags: string[]; createdAt: string; }
export interface Deal { id: string; title: string; contactId: string; value: number; stage: string; probability: number; closedAt: string | undefined; createdAt: string; }
export class CrmEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS contacts (id TEXT PRIMARY KEY, name TEXT, email TEXT, phone TEXT, company TEXT, tags TEXT DEFAULT '[]', created_at TEXT); CREATE TABLE IF NOT EXISTS deals (id TEXT PRIMARY KEY, title TEXT, contact_id TEXT, value REAL, stage TEXT DEFAULT 'lead', probability REAL DEFAULT 0, closed_at TEXT, created_at TEXT)`); }
  createContact(c: { name: string; email: string | undefined; phone: string | undefined; company: string | undefined; tags?: string[] }): Contact {
    const ct: Contact = { id: randomUUID(), name: c.name, email: c.email, phone: c.phone, company: c.company, tags: c.tags || [], createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO contacts VALUES (?,?,?,?,?,?,?)").run(ct.id, ct.name, ct.email||null, ct.phone||null, ct.company||null, JSON.stringify(ct.tags), ct.createdAt);
    return ct;
  }
  listContacts(): Contact[] { return (this.db.prepare("SELECT * FROM contacts ORDER BY created_at DESC").all() as Array<Record<string,unknown>>).map((r: Record<string,unknown>) => ({ ...r, tags: JSON.parse(r.tags as string) } as Contact)); }
  createDeal(d: { title: string; contactId: string; value: number; stage?: string; probability?: number }): Deal {
    const dl: Deal = { id: randomUUID(), title: d.title, contactId: d.contactId, value: d.value, stage: d.stage || "lead", probability: d.probability || 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO deals VALUES (?,?,?,?,?,?,?,?)").run(dl.id, dl.title, dl.contactId, dl.value, dl.stage, dl.probability, null, dl.createdAt);
    return dl;
  }
  listDeals(): Deal[] { return this.db.prepare("SELECT * FROM deals ORDER BY created_at DESC").all() as Deal[]; }
  getPipeline(): Array<{ stage: string; count: number; totalValue: number }> {
    return this.db.prepare("SELECT stage, COUNT(*) as count, COALESCE(SUM(value),0) as total_value FROM deals GROUP BY stage ORDER BY COUNT(*) DESC").all() as Array<{ stage: string; count: number; totalValue: number }>;
  }
}
