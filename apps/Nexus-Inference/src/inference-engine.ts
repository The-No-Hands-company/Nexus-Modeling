import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Model { id: string; name: string; version: string; type: string; status: "idle" | "loading" | "ready" | "error"; createdAt: string; }
export interface Prediction { id: string; modelId: string; input: string; output: string; latencyMs: number; createdAt: string; }
export class InferenceEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS models (id TEXT PRIMARY KEY, name TEXT, version TEXT, type TEXT, status TEXT DEFAULT 'idle', created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS predictions (id TEXT PRIMARY KEY, model_id TEXT, input TEXT, output TEXT, latency_ms REAL, created_at TEXT)");
  }
  registerModel(name: string, version: string, type: string): Model {
    const m: Model = { id: randomUUID(), name, version, type, status: "idle", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO models VALUES (?,?,?,?,?,?)").run(m.id, m.name, m.version, m.type, m.status, m.createdAt); return m;
  }
  listModels(): Model[] { return this.db.prepare("SELECT * FROM models ORDER BY created_at DESC").all() as Model[]; }
  getModel(id: string): Model | undefined { return this.db.prepare("SELECT * FROM models WHERE id = ?").get(id) as Model | undefined; }
  runPrediction(modelId: string, input: string): Prediction {
    const start = performance.now();
    const output = `[mock] processed "${input.slice(0, 50)}" via model ${modelId}`;
    const p: Prediction = { id: randomUUID(), modelId, input, output, latencyMs: Math.round(performance.now() - start), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO predictions VALUES (?,?,?,?,?,?)").run(p.id, p.modelId, p.input, p.output, p.latencyMs, p.createdAt); return p;
  }
  listPredictions(modelId?: string): Prediction[] {
    if (modelId) return this.db.prepare("SELECT * FROM predictions WHERE model_id = ? ORDER BY created_at DESC").all(modelId) as Prediction[];
    return this.db.prepare("SELECT * FROM predictions ORDER BY created_at DESC").all() as Prediction[];
  }
}
