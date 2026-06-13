import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Vertical { id: string; name: string; description: string; industry: string; active: number; createdAt: string; }
export interface Pipeline { id: string; verticalId: string; name: string; stages: string; dealCount: number; createdAt: string; }
export interface Stage { id: string; pipelineId: string; name: string; order: number; dealValue: number; createdAt: string; }
export class VerticalEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS verticals (id TEXT PRIMARY KEY, name TEXT, description TEXT, industry TEXT, active INTEGER DEFAULT 1, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS pipelines (id TEXT PRIMARY KEY, vertical_id TEXT, name TEXT, stages TEXT, deal_count INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS stages (id TEXT PRIMARY KEY, pipeline_id TEXT, name TEXT, stage_order INTEGER, deal_value REAL DEFAULT 0, created_at TEXT)"); }
  addVertical(name: string, description: string, industry: string): Vertical {
    const v: Vertical = { id: randomUUID(), name, description, industry, active: 1, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO verticals VALUES (?,?,?,?,?,?)").run(v.id, v.name, v.description, v.industry, v.active, v.createdAt); return v; }
  listVerticals(industry?: string): Vertical[] {
    if (industry) return this.db.prepare("SELECT * FROM verticals WHERE industry = ? ORDER BY name ASC").all(industry) as Vertical[];
    return this.db.prepare("SELECT * FROM verticals ORDER BY name ASC").all() as Vertical[]; }
  getVertical(id: string): Vertical | undefined { return this.db.prepare("SELECT * FROM verticals WHERE id = ?").get(id) as Vertical | undefined; }
  addPipeline(verticalId: string, name: string, stages: string): Pipeline {
    const p: Pipeline = { id: randomUUID(), verticalId, name, stages, dealCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO pipelines VALUES (?,?,?,?,?,?)").run(p.id, p.verticalId, p.name, p.stages, p.dealCount, p.createdAt); return p; }
  listPipelines(verticalId: string): Pipeline[] { return this.db.prepare("SELECT * FROM pipelines WHERE vertical_id = ? ORDER BY name ASC").all(verticalId) as Pipeline[]; }
  addStage(pipelineId: string, name: string, order: number, dealValue = 0): Stage {
    const s: Stage = { id: randomUUID(), pipelineId, name, order, dealValue, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO stages VALUES (?,?,?,?,?,?)").run(s.id, s.pipelineId, s.name, s.order, s.dealValue, s.createdAt); return s; }
  listStages(pipelineId: string): Stage[] { return this.db.prepare("SELECT * FROM stages WHERE pipeline_id = ? ORDER BY stage_order ASC").all(pipelineId) as Stage[]; } }
