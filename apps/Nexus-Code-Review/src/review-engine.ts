import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Review { id: string; title: string; code: string; language: string; authorId: string; status: "pending"|"reviewed"; feedback?: { reviewerId: string; verdict: string; comments: string; score: number; reviewedAt: string }; createdAt: string; }
export class CodeReviewEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS reviews (id TEXT PRIMARY KEY, title TEXT, code TEXT, language TEXT, author_id TEXT, status TEXT DEFAULT 'pending', feedback TEXT, created_at TEXT)`); }
  submitReview(r: { title: string; code: string; language?: string; authorId: string }): Review {
    const rv: Review = { id: randomUUID(), title: r.title, code: r.code, language: r.language || "unknown", authorId: r.authorId, status: "pending", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO reviews VALUES (?,?,?,?,?,?,?,?)").run(rv.id, rv.title, rv.code, rv.language, rv.authorId, rv.status, null, rv.createdAt);
    return rv;
  }
  listReviews(): Review[] {
    return (this.db.prepare("SELECT * FROM reviews ORDER BY created_at DESC").all() as Array<Record<string,unknown>>).map((r: Record<string,unknown>) => ({ ...r, feedback: r.feedback ? JSON.parse(r.feedback as string) : undefined } as Review));
  }
  getReview(id: string): Review | undefined {
    const r = this.db.prepare("SELECT * FROM reviews WHERE id = ?").get(id) as Record<string,unknown> | undefined;
    return r ? { ...r, feedback: r.feedback ? JSON.parse(r.feedback as string) : undefined } as Review : undefined;
  }
  addFeedback(id: string, fb: { reviewerId: string; verdict: string; comments: string; score: number }): Review | undefined {
    const r = this.getReview(id); if (!r) return undefined;
    r.feedback = { ...fb, reviewedAt: new Date().toISOString() }; r.status = "reviewed";
    this.db.prepare("UPDATE reviews SET status = 'reviewed', feedback = ? WHERE id = ?").run(JSON.stringify(r.feedback), id);
    return r;
  }
}
