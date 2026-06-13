import { Database } from "bun:sqlite";

export interface Dashboard { id: string; name: string; query: string; type: "bar"|"line"|"pie"|"table"|"number"; data: unknown; createdAt: string; }
export interface Metric { name: string; value: number; unit: string; timestamp: string; source: string; }

export class AnalyticsEngine {
  db: Database;
  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec(`CREATE TABLE IF NOT EXISTS metrics (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, value REAL, unit TEXT, timestamp TEXT, source TEXT); CREATE INDEX IF NOT EXISTS idx_m_name ON metrics(name)`);
  }

  track(name: string, value: number, unit = "count", source = "api"): void {
    this.db.prepare("INSERT INTO metrics (name, value, unit, timestamp, source) VALUES (?,?,?,?,?)").run(name, value, unit, new Date().toISOString(), source);
  }

  query(name: string, windowHours = 24): Metric[] {
    const since = new Date(Date.now() - windowHours * 3600000).toISOString();
    return this.db.prepare("SELECT * FROM metrics WHERE name = ? AND timestamp >= ? ORDER BY timestamp").all(name, since) as Metric[];
  }

  aggregate(name: string): { min: number; max: number; avg: number; count: number; sum: number } {
    const r = this.db.prepare("SELECT MIN(value) as min, MAX(value) as max, AVG(value) as avg, COUNT(*) as cnt, SUM(value) as sum FROM metrics WHERE name = ?").get(name) as Record<string, number> | undefined;
    return { min: r?.min || 0, max: r?.max || 0, avg: Math.round((r?.avg || 0) * 100) / 100, count: r?.cnt || 0, sum: r?.sum || 0 };
  }

  listMetrics(): string[] {
    return (this.db.prepare("SELECT DISTINCT name FROM metrics ORDER BY name").all() as Array<{ name: string }>).map((r: { name: string }) => r.name);
  }
}
