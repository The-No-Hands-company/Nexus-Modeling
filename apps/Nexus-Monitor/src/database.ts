import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import type {
  ActiveAlert,
  AlertRule,
  AlertHistoryEntry,
  MetricPoint,
  MetricStats,
  ServiceRecord,
  ServiceStatus,
  DashboardData,
} from "./types";

// ── Schema ────────────────────────────────────────────────────────────────────

const SCHEMA = /* sql */ `
PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS services (
  name        TEXT PRIMARY KEY,
  status      TEXT NOT NULL DEFAULT 'unknown',
  last_heartbeat TEXT NOT NULL,
  registered_at  TEXT NOT NULL,
  metadata    TEXT DEFAULT '{}',
  endpoint    TEXT
);

CREATE TABLE IF NOT EXISTS metrics (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  name        TEXT NOT NULL,
  value       REAL NOT NULL,
  tags        TEXT NOT NULL DEFAULT '{}',
  timestamp   TEXT NOT NULL,
  source      TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_metrics_name ON metrics(name);
CREATE INDEX IF NOT EXISTS idx_metrics_source ON metrics(source);
CREATE INDEX IF NOT EXISTS idx_metrics_timestamp ON metrics(timestamp);

CREATE TABLE IF NOT EXISTS alert_rules (
  id              TEXT PRIMARY KEY,
  metric          TEXT NOT NULL,
  condition       TEXT NOT NULL,
  threshold       REAL NOT NULL,
  tags            TEXT DEFAULT '{}',
  severity        TEXT NOT NULL DEFAULT 'info',
  description     TEXT NOT NULL DEFAULT '',
  cooldown_seconds INTEGER NOT NULL DEFAULT 300
);

CREATE TABLE IF NOT EXISTS active_alerts (
  rule_id       TEXT PRIMARY KEY REFERENCES alert_rules(id) ON DELETE CASCADE,
  fired_at      TEXT NOT NULL,
  last_value    REAL NOT NULL,
  acknowledged  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS alert_history (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  rule_id     TEXT NOT NULL,
  metric      TEXT NOT NULL,
  value       REAL NOT NULL,
  threshold   REAL NOT NULL,
  condition   TEXT NOT NULL,
  severity    TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  fired_at    TEXT NOT NULL,
  source      TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_alert_history_fired ON alert_history(fired_at);

CREATE TABLE IF NOT EXISTS events (
  id          TEXT PRIMARY KEY,
  kind        TEXT NOT NULL,
  source      TEXT NOT NULL,
  timestamp   TEXT NOT NULL,
  payload     TEXT NOT NULL DEFAULT '{}'
);
CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);

CREATE TABLE IF NOT EXISTS monitor_state (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
`;

// ── Database handle ───────────────────────────────────────────────────────────

export interface DatabaseOptions {
  /** Path to SQLite file. Defaults to :memory: for tests. */
  path?: string;
  /** Max metrics rows to retain (ring buffer behavior). Default 100_000. */
  maxMetrics?: number;
  /** Max event rows to retain. Default 10_000. */
  maxEvents?: number;
  /** Max seconds before a heartbeat is considered stale. Default 120. */
  heartbeatTimeoutSeconds?: number;
}

export class MonitorDatabase {
  private db: Database;
  readonly maxMetrics: number;
  readonly maxEvents: number;
  readonly heartbeatTimeoutSeconds: number;

  constructor(options: DatabaseOptions = {}) {
    this.db = new Database(options.path ?? ":memory:");
    this.maxMetrics = options.maxMetrics ?? 100_000;
    this.maxEvents = options.maxEvents ?? 10_000;
    this.heartbeatTimeoutSeconds = options.heartbeatTimeoutSeconds ?? 120;

    this.db.exec(SCHEMA);
    // Initialize counter state
    this.db
      .prepare("INSERT OR IGNORE INTO monitor_state (key, value) VALUES (?, ?)")
      .run("events_received", "0");
    this.db
      .prepare("INSERT OR IGNORE INTO monitor_state (key, value) VALUES (?, ?)")
      .run("heartbeats_received", "0");
    this.db
      .prepare("INSERT OR IGNORE INTO monitor_state (key, value) VALUES (?, ?)")
      .run("next_id", "1");
  }

  close(): void {
    this.db.close();
  }

  // ── State counters ────────────────────────────────────────────────────────

  get eventsReceived(): number {
    const row = this.db
      .prepare("SELECT value FROM monitor_state WHERE key = 'events_received'")
      .get() as { value: string } | undefined;
    return row ? Number.parseInt(row.value, 10) : 0;
  }

  incrementEventsReceived(): number {
    const next = this.eventsReceived + 1;
    this.db.prepare("UPDATE monitor_state SET value = ? WHERE key = 'events_received'").run(
      String(next),
    );
    return next;
  }

  get heartbeatsReceived(): number {
    const row = this.db
      .prepare("SELECT value FROM monitor_state WHERE key = 'heartbeats_received'")
      .get() as { value: string } | undefined;
    return row ? Number.parseInt(row.value, 10) : 0;
  }

  incrementHeartbeatsReceived(): number {
    const next = this.heartbeatsReceived + 1;
    this.db
      .prepare("UPDATE monitor_state SET value = ? WHERE key = 'heartbeats_received'")
      .run(String(next));
    return next;
  }

  getNextId(): string {
    const row = this.db
      .prepare("SELECT value FROM monitor_state WHERE key = 'next_id'")
      .get() as { value: string } | undefined;
    const n = row ? Number.parseInt(row.value, 10) : 1;
    this.db.prepare("UPDATE monitor_state SET value = ? WHERE key = 'next_id'").run(
      String(n + 1),
    );
    return `evt-${String(n).padStart(6, "0")}`;
  }

  // ── Services ──────────────────────────────────────────────────────────────

  getService(name: string): ServiceRecord | undefined {
    const row = this.db.prepare("SELECT * FROM services WHERE name = ?").get(name) as
      | Record<string, unknown>
      | undefined;
    if (!row) return undefined;
    return rowToService(row);
  }

  getAllServices(): ServiceRecord[] {
    const rows = this.db.prepare("SELECT * FROM services ORDER BY name").all() as Record<
      string,
      unknown
    >[];
    return rows.map(rowToService);
  }

  upsertService(record: ServiceRecord): void {
    this.db
      .prepare(
        `INSERT INTO services (name, status, last_heartbeat, registered_at, metadata, endpoint)
       VALUES (?, ?, ?, ?, ?, ?)
       ON CONFLICT(name) DO UPDATE SET
         status=excluded.status,
         last_heartbeat=excluded.last_heartbeat,
         metadata=excluded.metadata,
         endpoint=COALESCE(excluded.endpoint, services.endpoint)`,
      )
      .run(
        record.name,
        record.status,
        record.lastHeartbeat,
        record.registeredAt,
        JSON.stringify(record.metadata ?? {}),
        record.endpoint ?? null,
      );
  }

  getStaleHeartbeats(now: string): ServiceRecord[] {
    const rows = this.db
      .prepare(
        `SELECT * FROM services
       WHERE last_heartbeat < ?
       ORDER BY name`,
      )
      .all(now) as Record<string, unknown>[];
    return rows.map(rowToService);
  }

  // ── Metrics ───────────────────────────────────────────────────────────────

  insertMetric(point: MetricPoint): void {
    this.db
      .prepare(
        "INSERT INTO metrics (name, value, tags, timestamp, source) VALUES (?, ?, ?, ?, ?)",
      )
      .run(
        point.name,
        point.value,
        JSON.stringify(point.tags),
        point.timestamp,
        point.source,
      );

    // Enforce ring-buffer cap
    const count = (
      this.db.prepare("SELECT COUNT(*) as c FROM metrics").get() as { c: number }
    ).c;
    if (count > this.maxMetrics) {
      const excess = count - this.maxMetrics;
      this.db
        .prepare("DELETE FROM metrics WHERE id IN (SELECT id FROM metrics ORDER BY id LIMIT ?)")
        .run(excess);
    }
  }

  queryMetrics(opts: {
    name?: string;
    source?: string;
    limit?: number;
  }): { metrics: MetricPoint[]; total: number } {
    const conditions: string[] = [];
    const params: unknown[] = [];

    if (opts.name) {
      conditions.push("name = ?");
      params.push(opts.name);
    }
    if (opts.source) {
      conditions.push("source = ?");
      params.push(opts.source);
    }

    const where = conditions.length > 0 ? `WHERE ${conditions.join(" AND ")}` : "";
    const limit = Math.min(opts.limit ?? 100, 1000);

    const countRow = this.db
      .prepare(`SELECT COUNT(*) as c FROM metrics ${where}`)
      .get(...params as any) as { c: number };

    const rows = this.db
      .prepare(
        `SELECT * FROM metrics ${where} ORDER BY id DESC LIMIT ?`,
      )
      .all(...params as any, limit) as Record<string, unknown>[];

    return {
      metrics: rows.map(rowToMetric).reverse(),
      total: countRow.c,
    };
  }

  getMetricStats(
    name: string,
    windowSeconds: number,
    now: string,
  ): MetricStats {
    const since = new Date(Date.parse(now) - windowSeconds * 1000).toISOString();

    const rows = this.db
      .prepare(
        `SELECT value FROM metrics
       WHERE name = ? AND timestamp >= ?
       ORDER BY value`,
      )
      .all(name, since) as { value: number }[];

    const values = rows.map((r) => r.value);
    if (values.length === 0) {
      return { name, windowSeconds, count: 0, min: 0, max: 0, avg: 0, p95: 0, latest: 0 };
    }

    const count = values.length;
    const min = values[0]!;
    const max = values[count - 1]!;
    const sum = values.reduce((a, b) => a + b, 0);
    const avg = sum / count;
    const p95Idx = Math.ceil(count * 0.95) - 1;
    const p95 = values[p95Idx]!;

    const latestRow = this.db
      .prepare(
        "SELECT value FROM metrics WHERE name = ? ORDER BY id DESC LIMIT 1",
      )
      .get(name) as { value: number } | undefined;
    const latest = latestRow?.value ?? values[count - 1]!;

    return { name, windowSeconds, count, min, max, avg, p95, latest };
  }

  getMetricsCount(): number {
    return (this.db.prepare("SELECT COUNT(*) as c FROM metrics").get() as { c: number }).c;
  }

  // ── Alert Rules ───────────────────────────────────────────────────────────

  getAlertRule(id: string): AlertRule | undefined {
    const row = this.db.prepare("SELECT * FROM alert_rules WHERE id = ?").get(id) as
      | Record<string, unknown>
      | undefined;
    if (!row) return undefined;
    return rowToAlertRule(row);
  }

  getAllAlertRules(): AlertRule[] {
    const rows = this.db.prepare("SELECT * FROM alert_rules").all() as Record<string, unknown>[];
    return rows.map(rowToAlertRule);
  }

  insertAlertRule(rule: AlertRule): void {
    this.db
      .prepare(
        `INSERT INTO alert_rules (id, metric, condition, threshold, tags, severity, description, cooldown_seconds)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
      )
      .run(
        rule.id,
        rule.metric,
        rule.condition,
        rule.threshold,
        JSON.stringify(rule.tags ?? {}),
        rule.severity,
        rule.description,
        rule.cooldownSeconds,
      );
  }

  // ── Active Alerts ─────────────────────────────────────────────────────────

  getActiveAlert(ruleId: string): ActiveAlert | undefined {
    const row = this.db
      .prepare(
        `SELECT a.*, r.metric, r.condition, r.threshold, r.tags as rule_tags,
                r.severity, r.description, r.cooldown_seconds
       FROM active_alerts a
       JOIN alert_rules r ON a.rule_id = r.id
       WHERE a.rule_id = ?`,
      )
      .get(ruleId) as Record<string, unknown> | undefined;
    if (!row) return undefined;
    return rowToActiveAlert(row);
  }

  getAllActiveAlerts(): ActiveAlert[] {
    const rows = this.db
      .prepare(
        `SELECT a.*, r.metric, r.condition, r.threshold, r.tags as rule_tags,
                r.severity, r.description, r.cooldown_seconds
       FROM active_alerts a
       JOIN alert_rules r ON a.rule_id = r.id
       ORDER BY a.fired_at DESC`,
      )
      .all() as Record<string, unknown>[];
    return rows.map(rowToActiveAlert);
  }

  upsertActiveAlert(alert: ActiveAlert): void {
    this.db
      .prepare(
        `INSERT INTO active_alerts (rule_id, fired_at, last_value, acknowledged)
       VALUES (?, ?, ?, ?)
       ON CONFLICT(rule_id) DO UPDATE SET
         fired_at=excluded.fired_at,
         last_value=excluded.last_value,
         acknowledged=excluded.acknowledged`,
      )
      .run(alert.ruleId, alert.firedAt, alert.lastValue, alert.acknowledged ? 1 : 0);
  }

  acknowledgeAlert(ruleId: string): void {
    this.db
      .prepare("UPDATE active_alerts SET acknowledged = 1 WHERE rule_id = ?")
      .run(ruleId);
  }

  getActiveAlertsCount(): number {
    return (
      this.db.prepare("SELECT COUNT(*) as c FROM active_alerts").get() as { c: number }
    ).c;
  }

  // ── Alert History ─────────────────────────────────────────────────────────

  insertAlertHistory(alert: ActiveAlert, source: string): void {
    this.db
      .prepare(
        `INSERT INTO alert_history (rule_id, metric, value, threshold, condition, severity, description, fired_at, source)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      )
      .run(
        alert.ruleId,
        alert.rule.metric,
        alert.lastValue,
        alert.rule.threshold,
        alert.rule.condition,
        alert.rule.severity,
        alert.rule.description,
        alert.firedAt,
        source,
      );
  }

  getAlertHistory(opts: {
    ruleId?: string;
    severity?: string;
    limit?: number;
  }): AlertHistoryEntry[] {
    const conditions: string[] = [];
    const params: unknown[] = [];

    if (opts.ruleId) {
      conditions.push("rule_id = ?");
      params.push(opts.ruleId);
    }
    if (opts.severity) {
      conditions.push("severity = ?");
      params.push(opts.severity);
    }

    const where = conditions.length > 0 ? `WHERE ${conditions.join(" AND ")}` : "";
    const limit = Math.min(opts.limit ?? 100, 1000);

    const rows = this.db
      .prepare(
        `SELECT * FROM alert_history ${where} ORDER BY fired_at DESC LIMIT ?`,
      )
      .all(...params as any, limit) as Record<string, unknown>[];

    return rows.map(rowToAlertHistoryEntry);
  }

  // ── Events ────────────────────────────────────────────────────────────────

  insertEvent(event: { id: string; kind: string; source: string; timestamp: string; payload: unknown }): void {
    this.db
      .prepare(
        "INSERT INTO events (id, kind, source, timestamp, payload) VALUES (?, ?, ?, ?, ?)",
      )
      .run(event.id, event.kind, event.source, event.timestamp, JSON.stringify(event.payload));

    // Enforce cap
    const count = (
      this.db.prepare("SELECT COUNT(*) as c FROM events").get() as { c: number }
    ).c;
    if (count > this.maxEvents) {
      const excess = count - this.maxEvents;
      this.db
        .prepare("DELETE FROM events WHERE id IN (SELECT id FROM events ORDER BY timestamp LIMIT ?)")
        .run(excess);
    }
  }

  // ── Cleanup ───────────────────────────────────────────────────────────────

  expireHeartbeats(now: string): number {
    const expireThreshold = new Date(
      Date.parse(now) - this.heartbeatTimeoutSeconds * 1000,
    ).toISOString();

    const result = this.db
      .prepare(
        `UPDATE services SET status = 'unhealthy'
       WHERE last_heartbeat < ? AND status != 'unhealthy'`,
      )
      .run(expireThreshold);
    return result.changes;
  }

  vacuum(): void {
    this.db.exec("PRAGMA optimize");
  }
}

// ── Row mappers ──────────────────────────────────────────────────────────────

function parseJson(raw: unknown, fallback: Record<string, unknown> = {}): Record<string, unknown> {
  if (typeof raw === "string") {
    try {
      return JSON.parse(raw) as Record<string, unknown>;
    } catch {
      return fallback;
    }
  }
  return fallback;
}

function rowToService(row: Record<string, unknown>): ServiceRecord {
  return {
    name: row["name"] as string,
    status: (row["status"] as ServiceStatus) ?? "unknown",
    lastHeartbeat: row["last_heartbeat"] as string,
    registeredAt: row["registered_at"] as string,
    metadata: parseJson(row["metadata"]),
    endpoint: (row["endpoint"] as string) ?? undefined,
  };
}

function rowToMetric(row: Record<string, unknown>): MetricPoint {
  return {
    name: row["name"] as string,
    value: row["value"] as number,
    tags: parseJson(row["tags"]) as Record<string, string>,
    timestamp: row["timestamp"] as string,
    source: row["source"] as string,
  };
}

function rowToAlertRule(row: Record<string, unknown>): AlertRule {
  return {
    id: row["id"] as string,
    metric: row["metric"] as string,
    condition: row["condition"] as AlertRule["condition"],
    threshold: row["threshold"] as number,
    tags: parseJson(row["tags"]) as Record<string, string>,
    severity: (row["severity"] as AlertRule["severity"]) ?? "info",
    description: (row["description"] as string) ?? "",
    cooldownSeconds: row["cooldown_seconds"] as number,
  };
}

function rowToActiveAlert(row: Record<string, unknown>): ActiveAlert {
  return {
    ruleId: row["rule_id"] as string,
    firedAt: row["fired_at"] as string,
    lastValue: row["last_value"] as number,
    acknowledged: (row["acknowledged"] as number) === 1,
    rule: {
      id: row["rule_id"] as string,
      metric: row["metric"] as string,
      condition: row["condition"] as AlertRule["condition"],
      threshold: row["threshold"] as number,
      tags: parseJson(row["rule_tags"]) as Record<string, string>,
      severity: (row["severity"] as AlertRule["severity"]) ?? "info",
      description: (row["description"] as string) ?? "",
      cooldownSeconds: row["cooldown_seconds"] as number,
    },
  };
}

function rowToAlertHistoryEntry(row: Record<string, unknown>): AlertHistoryEntry {
  return {
    id: row["id"] as number,
    ruleId: row["rule_id"] as string,
    metric: row["metric"] as string,
    value: row["value"] as number,
    threshold: row["threshold"] as number,
    condition: row["condition"] as string,
    severity: row["severity"] as string,
    description: (row["description"] as string) ?? "",
    firedAt: row["fired_at"] as string,
    source: (row["source"] as string) ?? "",
  };
}

export type { MetricStats, DashboardData, AlertHistoryEntry } from "./types";
