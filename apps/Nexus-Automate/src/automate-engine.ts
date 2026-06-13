import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export type TriggerType = "webhook" | "schedule" | "event" | "manual";
export type ActionType = "http-request" | "send-email" | "webhook-out" | "log";

export interface Workflow { id: string; name: string; trigger: { type: TriggerType; config: Record<string, string> }; actions: Array<{ type: ActionType; config: Record<string, string> }>; enabled: boolean; runCount: number; lastRunAt?: string; createdAt: string; }

export class AutomateEngine {
  db: Database;
  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec(`CREATE TABLE IF NOT EXISTS workflows (id TEXT PRIMARY KEY, name TEXT, trigger_json TEXT, actions_json TEXT, enabled INTEGER DEFAULT 1, run_count INTEGER DEFAULT 0, last_run_at TEXT, created_at TEXT); CREATE TABLE IF NOT EXISTS run_log (id TEXT PRIMARY KEY, workflow_id TEXT, status TEXT, started_at TEXT, ended_at TEXT, result TEXT DEFAULT '{}')`);
  }

  createWorkflow(name: string, trigger: Workflow["trigger"], actions: Workflow["actions"]): Workflow {
    const w: Workflow = { id: randomUUID(), name, trigger, actions, enabled: true, runCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO workflows (id, name, trigger_json, actions_json, enabled, run_count, created_at) VALUES (?,?,?,?,?,?,?)").run(w.id, w.name, JSON.stringify(w.trigger), JSON.stringify(w.actions), 1, 0, w.createdAt);
    return w;
  }

  getWorkflow(id: string): Workflow | undefined {
    const r = this.db.prepare("SELECT * FROM workflows WHERE id = ?").get(id) as Record<string, unknown> | undefined;
    if (!r) return undefined;
    return { id: r.id as string, name: r.name as string, trigger: JSON.parse(r.trigger_json as string), actions: JSON.parse(r.actions_json as string), enabled: (r.enabled as number) === 1, runCount: r.run_count as number, lastRunAt: r.last_run_at as string, createdAt: r.created_at as string };
  }

  listWorkflows(): Workflow[] {
    const rows = this.db.prepare("SELECT * FROM workflows ORDER BY created_at DESC").all() as Array<Record<string, unknown>>;
    return rows.map((r: Record<string, unknown>) => ({ id: r.id as string, name: r.name as string, trigger: JSON.parse(r.trigger_json as string), actions: JSON.parse(r.actions_json as string), enabled: (r.enabled as number) === 1, runCount: r.run_count as number, lastRunAt: r.last_run_at as string, createdAt: r.created_at as string }));
  }

  setEnabled(id: string, enabled: boolean): boolean {
    return this.db.prepare("UPDATE workflows SET enabled = ? WHERE id = ?").run(enabled ? 1 : 0, id).changes > 0;
  }

  async executeWorkflow(id: string): Promise<{ steps: number; errors: number }> {
    const w = this.getWorkflow(id);
    if (!w || !w.enabled) return { steps: 0, errors: 0 };

    const runId = randomUUID();
    const startedAt = new Date().toISOString();
    this.db.prepare("INSERT INTO run_log (id, workflow_id, status, started_at) VALUES (?,?,?,?)").run(runId, id, "running", startedAt);

    let errors = 0;
    for (const action of w.actions) {
      try {
        if (action.type === "http-request") {
          await fetch(action.config.url || "http://localhost", { method: action.config.method || "GET", signal: AbortSignal.timeout(5000) });
        } else if (action.type === "webhook-out") {
          await fetch(action.config.webhookUrl || "", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ workflow: w.name, timestamp: startedAt }), signal: AbortSignal.timeout(5000) });
        }
      } catch { errors++; }
    }

    this.db.prepare("UPDATE workflows SET run_count = run_count + 1, last_run_at = ? WHERE id = ?").run(startedAt, id);
    this.db.prepare("UPDATE run_log SET status = ?, ended_at = ?, result = ? WHERE id = ?").run(errors === 0 ? "completed" : "partial", new Date().toISOString(), JSON.stringify({ steps: w.actions.length, errors }), runId);
    return { steps: w.actions.length, errors };
  }

  getRunLog(workflowId: string, limit = 20): Array<{ id: string; status: string; startedAt: string; result: unknown }> {
    return (this.db.prepare("SELECT * FROM run_log WHERE workflow_id = ? ORDER BY started_at DESC LIMIT ?").all(workflowId, limit) as Array<Record<string, unknown>>).map((r: Record<string, unknown>) => ({ id: r.id as string, status: r.status as string, startedAt: r.started_at as string, result: JSON.parse(r.result as string) }));
  }
}
