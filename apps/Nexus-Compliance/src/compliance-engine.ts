import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface ComplianceEvent { id: string; type: string; description: string; severity: string; source: string; data?: Record<string,unknown>; recordedAt: string; }
export interface ComplianceRule { id: string; name: string; regulation: string; description: string; required: boolean; createdAt: string; }
export class ComplianceEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); this.db.exec(`CREATE TABLE IF NOT EXISTS events (id TEXT PRIMARY KEY, type TEXT, description TEXT, severity TEXT, source TEXT, data TEXT DEFAULT '{}', recorded_at TEXT); CREATE TABLE IF NOT EXISTS rules (id TEXT PRIMARY KEY, name TEXT, regulation TEXT, description TEXT, required INTEGER DEFAULT 1, created_at TEXT)`); }
  recordEvent(e: { type: string; description: string; severity: string; source: string; data?: Record<string,unknown> }): ComplianceEvent {
    const ev: ComplianceEvent = { id: randomUUID(), ...e, recordedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO events VALUES (?,?,?,?,?,?,?)").run(ev.id, ev.type, ev.description, ev.severity, ev.source, JSON.stringify(ev.data||{}), ev.recordedAt);
    return ev;
  }
  listEvents(): ComplianceEvent[] { return this.db.prepare("SELECT * FROM events ORDER BY recorded_at DESC LIMIT 100").all() as ComplianceEvent[]; }
  generateReport(): { totalEvents: number; byType: Record<string,number>; bySeverity: Record<string,number>; recentEvents: ComplianceEvent[] } {
    const total = (this.db.prepare("SELECT COUNT(*) as c FROM events").get() as {c:number}).c;
    const byType: Record<string,number> = {}; const bySeverity: Record<string,number> = {};
    for (const e of this.listEvents()) { byType[e.type] = (byType[e.type]||0)+1; bySeverity[e.severity] = (bySeverity[e.severity]||0)+1; }
    return { totalEvents: total, byType, bySeverity, recentEvents: this.listEvents().slice(0, 10) };
  }
  createRule(r: { name: string; regulation: string; description?: string; required?: boolean }): ComplianceRule {
    const rule: ComplianceRule = { id: randomUUID(), name: r.name, regulation: r.regulation, description: r.description || "", required: r.required !== false, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO rules VALUES (?,?,?,?,?,?)").run(rule.id, rule.name, rule.regulation, rule.description, rule.required?1:0, rule.createdAt);
    return rule;
  }
  listRules(): ComplianceRule[] { return this.db.prepare("SELECT * FROM rules ORDER BY created_at DESC").all() as ComplianceRule[]; }
}
