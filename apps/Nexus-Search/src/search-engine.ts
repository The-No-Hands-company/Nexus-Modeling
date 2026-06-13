import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface SearchDocument {
  id: string;
  title: string;
  content: string;
  source: string;
  url: string | undefined;
  metadata: Record<string, string>;
  indexedAt: string;
  updatedAt: string;
}

export interface SearchResult {
  id: string;
  title: string;
  content: string;
  source: string;
  url: string | undefined;
  score: number;
  snippet: string;
}

export interface SearchIndexStats {
  totalDocuments: number;
  totalTerms: number;
  sources: { name: string; count: number }[];
  lastIndexedAt: string;
}

export interface FederatedSource {
  id: string;
  name: string;
  url: string;
  type: "files" | "wiki" | "notes" | "custom";
  enabled: boolean;
  syncIntervalMs: number;
  lastSyncAt: string | undefined;
  authToken: string | undefined;
  createdAt: string;
}

export interface SearchOptions {
  source: string | undefined;
  limit: number;
  offset: number;
  sort: "relevance" | "date";
  facets: boolean;
}

export interface SearchResponse {
  results: SearchResult[];
  total: number;
  limit: number;
  offset: number;
  facets: Record<string, Array<{ value: string; count: number }>> | undefined;
  took: number;
}

export class SearchEngine {
  private db: Database;

  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec("PRAGMA journal_mode=WAL");
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS documents (
        id TEXT PRIMARY KEY, title TEXT NOT NULL, content TEXT NOT NULL,
        source TEXT NOT NULL, url TEXT, metadata TEXT DEFAULT '{}',
        indexedAt TEXT NOT NULL, updatedAt TEXT NOT NULL
      );
      CREATE VIRTUAL TABLE IF NOT EXISTS doc_fts USING fts5(
        title, content, source,
        tokenize='porter unicode61',
        prefix='2 3'
      );
      CREATE TABLE IF NOT EXISTS sources (
        id TEXT PRIMARY KEY, name TEXT NOT NULL, url TEXT NOT NULL,
        type TEXT NOT NULL DEFAULT 'custom', enabled INTEGER DEFAULT 1,
        sync_interval_ms INTEGER DEFAULT 300000,
        last_sync_at TEXT, auth_token TEXT, created_at TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS stats (
        key TEXT PRIMARY KEY, value TEXT NOT NULL
      );
    `);
  }

  indexDocument(doc: {
    title: string; content: string; source: string;
    url: string | undefined; metadata: Record<string, string>;
    id: string | undefined;
  }): SearchDocument {
    const id = doc.id || randomUUID();
    const now = new Date().toISOString();

    const existing = this.db.prepare("SELECT id FROM documents WHERE id = ?").get(id);
    if (existing) {
      this.db.prepare("DELETE FROM doc_fts WHERE rowid = (SELECT rowid FROM documents WHERE id = ?)").run(id);
    }

    this.db.prepare(`
      INSERT OR REPLACE INTO documents (id, title, content, source, url, metadata, indexedAt, updatedAt)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `).run(id, doc.title, doc.content, doc.source, doc.url || null, JSON.stringify(doc.metadata || {}), now, now);

    this.db.prepare(`
      INSERT INTO doc_fts (rowid, title, content, source)
      VALUES ((SELECT rowid FROM documents WHERE id = ?), ?, ?, ?)
    `).run(id, doc.title, doc.content, doc.source);

    this.db.prepare("INSERT OR REPLACE INTO stats (key, value) VALUES ('lastIndexedAt', ?)").run(now);
    return { id, title: doc.title, content: doc.content, source: doc.source, url: doc.url || undefined, metadata: doc.metadata || {}, indexedAt: now, updatedAt: now };
  }

  indexDocuments(docs: Array<{
    title: string; content: string; source: string;
    url: string | undefined; metadata: Record<string, string>;
    id: string | undefined;
  }>): SearchDocument[] {
    return docs.map((d) => this.indexDocument(d));
  }

  search(query: string, options: SearchOptions): SearchResponse {
    const start = Date.now();
    const limit = Math.min(options.limit, 100);
    const offset = options.offset;

    let where = "doc_fts MATCH ?";
    const params: unknown[] = [this.buildFtsQuery(query)];

    if (options.source) {
      where += " AND d.source = ?";
      params.push(options.source);
    }

    const countRow = this.db.prepare(`
      SELECT COUNT(*) as c FROM doc_fts f JOIN documents d ON d.rowid = f.rowid WHERE ${where}
    `).get(...params as any) as { c: number };

    const orderBy = options.sort === "date" ? "d.indexedAt DESC" : "_rank";

    const rows = this.db.prepare(`
      SELECT d.*, f.rank as _rank
      FROM doc_fts f JOIN documents d ON d.rowid = f.rowid
      WHERE ${where}
      ORDER BY ${orderBy}
      LIMIT ? OFFSET ?
    `).all(...params as any, limit, offset) as Array<Record<string, unknown>>;

    const results: SearchResult[] = rows.map((row) => {
      const content = (row["content"] as string) || "";
      return {
        id: row["id"] as string,
        title: (row["title"] as string) || "",
        content,
        source: (row["source"] as string) || "",
        url: (row["url"] as string) || undefined,
        score: -(row["_rank"] as number),
        snippet: this.generateSnippet(content, query, 150),
      };
    });

    let facets: Record<string, Array<{ value: string; count: number }>> | undefined;
    if (options.facets) {
      const sourceRows = this.db.prepare(`
        SELECT d.source as value, COUNT(*) as count
        FROM doc_fts f JOIN documents d ON d.rowid = f.rowid
        WHERE ${where}
        GROUP BY d.source ORDER BY count DESC
      `).all(...params as any) as Array<{ value: string; count: number }>;
      facets = { source: sourceRows };
    }

    return { results, total: countRow.c, limit, offset, facets, took: Date.now() - start };
  }

  // ── Federated Source Management ──

  addSource(input: {
    name: string; url: string;
    type: FederatedSource["type"];
    syncIntervalMs: number;
    authToken: string | undefined;
  }): FederatedSource {
    const id = randomUUID();
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO sources (id, name, url, type, enabled, sync_interval_ms, auth_token, created_at)
      VALUES (?, ?, ?, ?, 1, ?, ?, ?)
    `).run(id, input.name, input.url, input.type, input.syncIntervalMs || 300000, input.authToken || null, now);
    return { id, name: input.name, url: input.url, type: input.type, enabled: true, syncIntervalMs: input.syncIntervalMs || 300000, lastSyncAt: undefined, authToken: input.authToken, createdAt: now };
  }

  getSource(id: string): FederatedSource | undefined {
    const row = this.db.prepare("SELECT * FROM sources WHERE id = ?").get(id) as Record<string, unknown> | undefined;
    return row ? this.rowToSource(row) : undefined;
  }

  listSources(): FederatedSource[] {
    return (this.db.prepare("SELECT * FROM sources ORDER BY name").all() as Array<Record<string, unknown>>).map((r) => this.rowToSource(r));
  }

  setSourceEnabled(id: string, enabled: boolean): void {
    this.db.prepare("UPDATE sources SET enabled = ? WHERE id = ?").run(enabled ? 1 : 0, id);
  }

  deleteSource(id: string): boolean {
    const result = this.db.prepare("DELETE FROM sources WHERE id = ?").run(id);
    return result.changes > 0;
  }

  updateSourceLastSync(id: string): void {
    this.db.prepare("UPDATE sources SET last_sync_at = ? WHERE id = ?").run(new Date().toISOString(), id);
  }

  getDocumentsForSource(source: string): SearchDocument[] {
    return (this.db.prepare("SELECT * FROM documents WHERE source = ? ORDER BY indexedAt DESC LIMIT 500").all(source) as Array<Record<string, unknown>>).map(this.rowToDoc);
  }

  deleteDocumentsFromSource(source: string): number {
    const docs = this.db.prepare("SELECT rowid FROM documents WHERE source = ?").all(source) as Array<{ rowid: number }>;
    for (const doc of docs) this.db.prepare("DELETE FROM doc_fts WHERE rowid = ?").run(doc.rowid);
    const result = this.db.prepare("DELETE FROM documents WHERE source = ?").run(source);
    return result.changes;
  }

  // ── Sync Engine ──

  async syncSource(source: FederatedSource): Promise<{ indexed: number; error?: string }> {
    try {
      const headers: Record<string, string> = { accept: "application/json" };
      if (source.authToken) headers["authorization"] = `Bearer ${source.authToken}`;

      let endpoint = source.url;
      if (source.type === "files") endpoint = `${source.url.replace(/\/$/, "")}/api/v1/files`;
      else if (source.type === "wiki") endpoint = `${source.url.replace(/\/$/, "")}/api/v1/articles`;
      else if (source.type === "notes") endpoint = `${source.url.replace(/\/$/, "")}/api/v1/notes`;

      const res = await fetch(endpoint, { headers, signal: AbortSignal.timeout(10000) });
      if (!res.ok) return { indexed: 0, error: `HTTP ${res.status}` };

      const data = await res.json() as { files?: Array<{ id: string; name: string; contentType: string }>; articles?: Array<{ id: string; title: string; content: string }>; notes?: Array<{ id: string; title: string; content: string }> };
      const items = data.files || data.articles || data.notes || [];
      if (!Array.isArray(items) || items.length === 0) return { indexed: 0 };

      this.deleteDocumentsFromSource(source.name);

      const docs = items.map((item: Record<string, unknown>) => ({
        title: (item.name || item.title || "Untitled") as string,
        content: (item.content || item.contentType || item.name || "") as string,
        source: source.name,
        url: source.url,
        metadata: {},
        id: undefined,
      }));

      this.indexDocuments(docs);
      this.updateSourceLastSync(source.id);

      return { indexed: docs.length };
    } catch (err) {
      return { indexed: 0, error: (err as Error).message };
    }
  }

  async syncAllSources(): Promise<Array<{ source: string; indexed: number; error?: string }>> {
    const sources = this.listSources().filter((s) => s.enabled);
    const results = [];
    for (const source of sources) {
      results.push({ source: source.name, ...(await this.syncSource(source)) });
    }
    return results;
  }

  // ── Queries ──

  getDocument(id: string): SearchDocument | undefined {
    const row = this.db.prepare("SELECT * FROM documents WHERE id = ?").get(id) as Record<string, unknown> | undefined;
    return row ? this.rowToDoc(row) : undefined;
  }

  getDocuments(opts: { source: string | undefined; limit: number; offset: number }): SearchDocument[] {
    const limit = Math.min(opts.limit, 500);
    const offset = opts.offset;

    if (opts.source) {
      return (this.db.prepare("SELECT * FROM documents WHERE source = ? ORDER BY updatedAt DESC LIMIT ? OFFSET ?").all(opts.source, limit, offset) as Array<Record<string, unknown>>).map(this.rowToDoc);
    }
    return (this.db.prepare("SELECT * FROM documents ORDER BY updatedAt DESC LIMIT ? OFFSET ?").all(limit, offset) as Array<Record<string, unknown>>).map(this.rowToDoc);
  }

  deleteDocument(id: string): boolean {
    const existing = this.db.prepare("SELECT rowid FROM documents WHERE id = ?").get(id) as { rowid: number } | undefined;
    if (!existing) return false;
    this.db.prepare("DELETE FROM doc_fts WHERE rowid = ?").run(existing.rowid);
    this.db.prepare("DELETE FROM documents WHERE id = ?").run(id);
    return true;
  }

  getStats(): SearchIndexStats {
    const sources = this.db.prepare("SELECT source as name, COUNT(*) as count FROM documents GROUP BY source ORDER BY count DESC").all() as Array<{ name: string; count: number }>;
    const docCount = (this.db.prepare("SELECT COUNT(*) as c FROM documents").get() as { c: number }).c;
    const termCount = (this.db.prepare("SELECT COUNT(*) as c FROM doc_fts").get() as { c: number }).c;
    const lastRow = this.db.prepare("SELECT value FROM stats WHERE key = 'lastIndexedAt'").get() as { value: string } | undefined;
    return { totalDocuments: docCount, totalTerms: termCount, sources, lastIndexedAt: lastRow?.value || "" };
  }

  clear(): void {
    this.db.exec("DELETE FROM doc_fts"); this.db.exec("DELETE FROM documents");
    this.db.exec("DELETE FROM sources"); this.db.exec("DELETE FROM stats");
  }

  close(): void { this.db.close(); }

  // ── Helpers ──

  private buildFtsQuery(query: string): string {
    const terms = query.split(/\s+/).filter((t) => t.length > 0);
    return terms.map((t) => `"${t.replace(/"/g, "")}"*`).join(" AND ");
  }

  private generateSnippet(content: string, query: string, maxLen: number): string {
    const terms = query.toLowerCase().split(/\s+/).filter((t) => t.length > 1);
    const lower = content.toLowerCase();
    let bestPos = 0;
    for (const term of terms) {
      const pos = lower.indexOf(term);
      if (pos >= 0) { bestPos = Math.max(0, pos - 40); break; }
    }
    let snippet = content.slice(bestPos, bestPos + maxLen);
    if (bestPos > 0) snippet = "…" + snippet;
    if (bestPos + maxLen < content.length) snippet += "…";
    return snippet;
  }

  private rowToDoc(row: Record<string, unknown>): SearchDocument {
    return {
      id: row["id"] as string, title: (row["title"] as string) || "",
      content: (row["content"] as string) || "", source: (row["source"] as string) || "",
      url: (row["url"] as string) || undefined,
      metadata: JSON.parse((row["metadata"] as string) || "{}") as Record<string, string>,
      indexedAt: row["indexedAt"] as string, updatedAt: row["updatedAt"] as string,
    };
  }

  private rowToSource(row: Record<string, unknown>): FederatedSource {
    return {
      id: row["id"] as string, name: row["name"] as string, url: row["url"] as string,
      type: (row["type"] as FederatedSource["type"]) || "custom",
      enabled: (row["enabled"] as number) === 1,
      syncIntervalMs: row["sync_interval_ms"] as number,
      lastSyncAt: row["last_sync_at"] as string || undefined,
      authToken: row["auth_token"] as string || undefined,
      createdAt: row["created_at"] as string,
    };
  }
}
