import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Keyword { id: string; keyword: string; volume: number; difficulty: number; url: string; createdAt: string; }
export interface Rank { id: string; keywordId: string; position: number; url: string; date: string; createdAt: string; }
export interface Page { id: string; url: string; title: string; score: number; lastCrawled: string; createdAt: string; }
export class SEOEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS keywords (id TEXT PRIMARY KEY, keyword TEXT, volume INTEGER DEFAULT 0, difficulty REAL, url TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS ranks (id TEXT PRIMARY KEY, keyword_id TEXT, position INTEGER, url TEXT, date TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS pages (id TEXT PRIMARY KEY, url TEXT, title TEXT, score REAL, last_crawled TEXT, created_at TEXT)"); }
  addKeyword(keyword: string, volume: number, difficulty: number, url: string): Keyword {
    const k: Keyword = { id: randomUUID(), keyword, volume, difficulty, url, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO keywords VALUES (?,?,?,?,?,?)").run(k.id, k.keyword, k.volume, k.difficulty, k.url, k.createdAt); return k; }
  listKeywords(): Keyword[] { return this.db.prepare("SELECT * FROM keywords ORDER BY volume DESC").all() as Keyword[]; }
  addRank(keywordId: string, position: number, url: string, date: string): Rank {
    const r: Rank = { id: randomUUID(), keywordId, position, url, date, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO ranks VALUES (?,?,?,?,?,?)").run(r.id, r.keywordId, r.position, r.url, r.date, r.createdAt); return r; }
  getRanks(keywordId: string): Rank[] { return this.db.prepare("SELECT * FROM ranks WHERE keyword_id = ? ORDER BY date DESC").all(keywordId) as Rank[]; }
  addPage(url: string, title: string, score: number): Page {
    const p: Page = { id: randomUUID(), url, title, score, lastCrawled: new Date().toISOString(), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO pages VALUES (?,?,?,?,?,?)").run(p.id, p.url, p.title, p.score, p.lastCrawled, p.createdAt); return p; }
  listPages(): Page[] { return this.db.prepare("SELECT * FROM pages ORDER BY score DESC").all() as Page[]; } }
