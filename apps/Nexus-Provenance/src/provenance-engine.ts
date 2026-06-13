import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface ProvenanceRecord { id: string; entityId: string; entityType: string; action: string; actor: string; details: string; chainId: string; createdAt: string; }
export interface Chain { id: string; name: string; description: string; recordCount: number; createdAt: string; }
export class ProvenanceEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS provenance_records (id TEXT PRIMARY KEY, entity_id TEXT, entity_type TEXT, action TEXT, actor TEXT, details TEXT, chain_id TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS chains (id TEXT PRIMARY KEY, name TEXT, description TEXT, record_count INTEGER DEFAULT 0, created_at TEXT)"); }
  addRecord(entityId: string, entityType: string, action: string, actor: string, details = "", chainId = ""): ProvenanceRecord {
    const r: ProvenanceRecord = { id: randomUUID(), entityId, entityType, action, actor, details, chainId, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO provenance_records VALUES (?,?,?,?,?,?,?,?)").run(r.id, r.entityId, r.entityType, r.action, r.actor, r.details, r.chainId, r.createdAt);
    if (chainId) this.db.prepare("UPDATE chains SET record_count = record_count + 1 WHERE id = ?").run(chainId); return r; }
  listRecords(entityId?: string, chainId?: string): ProvenanceRecord[] {
    let q = "SELECT * FROM provenance_records WHERE 1=1", p: any[] = [];
    if (entityId) { q += " AND entity_id = ?"; p.push(entityId); }
    if (chainId) { q += " AND chain_id = ?"; p.push(chainId); }
    q += " ORDER BY created_at DESC"; return this.db.prepare(q).all(...p) as ProvenanceRecord[]; }
  getRecord(id: string): ProvenanceRecord | undefined { return this.db.prepare("SELECT * FROM provenance_records WHERE id = ?").get(id) as ProvenanceRecord | undefined; }
  addChain(name: string, description = ""): Chain {
    const c: Chain = { id: randomUUID(), name, description, recordCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO chains VALUES (?,?,?,?,?)").run(c.id, c.name, c.description, c.recordCount, c.createdAt); return c; }
  listChains(): Chain[] { return this.db.prepare("SELECT * FROM chains ORDER BY name ASC").all() as Chain[]; } }
