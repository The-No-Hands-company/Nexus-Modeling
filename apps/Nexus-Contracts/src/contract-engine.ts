import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Contract { signedByA: string | undefined; signedByB: string | undefined; expiresAt: string | undefined;  id: string; title: string; partyA: string; partyB: string; content: string; status: "draft"|"signed_a"|"signed_b"|"active"|"expired"; signedByA?: string; signedByB?: string; expiresAt?: string; createdAt: string; }
export class ContractEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS contracts (id TEXT PRIMARY KEY, title TEXT, party_a TEXT, party_b TEXT, content TEXT, status TEXT DEFAULT 'draft', signed_by_a TEXT, signed_by_b TEXT, expires_at TEXT, created_at TEXT)`); }
  createContract(c: { title: string; partyA: string; partyB: string; content?: string; expiresAt?: string }): Contract {
    const ct: Contract = { id: randomUUID(), title: c.title, partyA: c.partyA, partyB: c.partyB, content: c.content || "", status: "draft", expiresAt: c.expiresAt, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO contracts VALUES (?,?,?,?,?,?,?,?,?,?)").run(ct.id, ct.title, ct.partyA, ct.partyB, ct.content, ct.status, null, null, ct.expiresAt || null, ct.createdAt);
    return ct;
  }
  listContracts(): Contract[] { return this.db.prepare("SELECT * FROM contracts ORDER BY created_at DESC").all() as Contract[]; }
  getContract(id: string): Contract | undefined { return this.db.prepare("SELECT * FROM contracts WHERE id = ?").get(id) as Contract | undefined; }
  signContract(id: string, party: string): Contract | undefined {
    const c = this.getContract(id); if (!c) return undefined;
    const now = new Date().toISOString();
    if (party === "a") { c.signedByA = now; c.status = c.signedByB ? "active" : "signed_a"; }
    else { c.signedByB = now; c.status = c.signedByA ? "active" : "signed_b"; }
    this.db.prepare("UPDATE contracts SET status = ?, signed_by_a = ?, signed_by_b = ? WHERE id = ?").run(c.status, c.signedByA || null, c.signedByB || null, id);
    return c;
  }
}
