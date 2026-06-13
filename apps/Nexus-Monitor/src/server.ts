import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { randomUUID } from "node:crypto";
import { MonitorDatabase } from "./database";
import { startNexusMonitorHeartbeat } from "./cloud";
import type {
  ServiceRecord,
  ServiceStatus,
  AlertRule,
  MetricPoint,
  MetricStats,
  DashboardData,
  AlertHistoryEntry,
} from "./types";

interface ServerHandle {
  server: ReturnType<typeof Bun.serve>;
  close: () => void;
}

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export function createMonitorServer(): ServerHandle {
  const port = Number(process.env.PORT || "3030");
  const baseUrl = process.env.NEXUS_MONITOR_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();

  const db = new MonitorDatabase({
    path: process.env.NEXUS_MONITOR_DB_PATH || "data/monitor.sqlite",
  });

  const expireTimer = setInterval(() => {
    const now = new Date().toISOString();
    db.expireHeartbeats(now);
  }, 30000);

  if (typeof expireTimer.unref === "function") expireTimer.unref();

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-monitor",
  name: "Nexus Monitor",
  description: "Ecosystem observability",
  port: 3030,
  capabilities: ["observability","metrics-ingestion","alerting","service-tracking","dashboard"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({
          service: "nexus-monitor",
          status: "ok",
          version: "v2",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          timestamp: new Date().toISOString(),
        }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/monitor/status") {
        return json({
          status: "ok",
          eventsReceived: db.eventsReceived,
          heartbeatsReceived: db.heartbeatsReceived,
          metricsStored: db.getMetricsCount(),
          activeAlerts: db.getActiveAlertsCount(),
          alertRules: db.getAllAlertRules().length,
          services: db.getAllServices().length,
        }, 200);
      }

      // ── Dashboard ──
      if (request.method === "GET" && path === "/api/v1/monitor/dashboard") {
        const services = db.getAllServices();
        const data: DashboardData = {
          timestamp: new Date().toISOString(),
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          overview: {
            servicesTotal: services.length,
            servicesHealthy: services.filter((s) => s.status === "healthy").length,
            servicesDegraded: services.filter((s) => s.status === "degraded").length,
            servicesUnhealthy: services.filter((s) => s.status === "unhealthy").length,
            eventsReceived: db.eventsReceived,
            heartbeatsReceived: db.heartbeatsReceived,
            metricsStored: db.getMetricsCount(),
            activeAlerts: db.getActiveAlertsCount(),
            alertRulesDefined: db.getAllAlertRules().length,
          },
          recentAlerts: db.getAllActiveAlerts().slice(0, 10),
          serviceStatuses: Object.fromEntries(
            services.map((s) => [s.name, { status: s.status, lastHeartbeat: s.lastHeartbeat }]),
          ),
        };
        return json(data, 200);
      }

      // ── Ingest ──
      if (request.method === "POST" && path === "/api/v1/monitor/ingest") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const source = typeof body.source === "string" ? body.source.trim() : "";
        const kind = typeof body.kind === "string" ? body.kind : "";
        if (!source || !["heartbeat", "alert", "event", "metric"].includes(kind)) {
          return json({ error: "source and kind (heartbeat|alert|event|metric) are required" }, 400);
        }

        const timestamp = typeof body.timestamp === "string" ? body.timestamp : new Date().toISOString();
        const id = db.getNextId();
        db.incrementEventsReceived();

        if (kind === "heartbeat") {
          db.incrementHeartbeatsReceived();
          const state = typeof body.state === "string" ? body.state : "unknown";
          const record: ServiceRecord = {
            name: source,
            status: state as ServiceStatus,
            lastHeartbeat: timestamp,
            registeredAt: timestamp,
            metadata: typeof body.metadata === "object" ? body.metadata as Record<string, unknown> : {},
            endpoint: typeof body.endpoint === "string" ? body.endpoint : undefined,
          };
          db.upsertService(record);
        }

        if (kind === "metric") {
          const name = typeof body.name === "string" ? body.name : "";
          const value = typeof body.value === "number" ? body.value : 0;
          if (name) {
            const point: MetricPoint = {
              name,
              value,
              tags: (typeof body.tags === "object" ? body.tags : {}) as Record<string, string>,
              timestamp,
              source,
            };
            db.insertMetric(point);

            // Evaluate alert rules
            const rules = db.getAllAlertRules().filter((r) => r.metric === name);
            for (const rule of rules) {
              const triggered =
                rule.condition === "gt" ? value > rule.threshold :
                rule.condition === "lt" ? value < rule.threshold :
                rule.condition === "gte" ? value >= rule.threshold :
                rule.condition === "lte" ? value <= rule.threshold : false;

              if (triggered) {
                const existing = db.getActiveAlert(rule.id);
                if (!existing || Date.now() - new Date(existing.firedAt).getTime() > rule.cooldownSeconds * 1000) {
                  db.upsertActiveAlert({ ruleId: rule.id, firedAt: timestamp, lastValue: value, acknowledged: false, rule });
                  db.insertAlertHistory({ ruleId: rule.id, firedAt: timestamp, lastValue: value, acknowledged: false, rule }, source);
                  nexusClient.evaluateWithGuardian({ scope: "service", subjectId: source, context: { alertMetric: rule.metric, alertValue: String(value), alertSeverity: rule.severity } }).catch(() => {});
                }
              }
            }
          }
        }

        db.insertEvent({ id, kind, source, timestamp, payload: body });
        return json({ status: "accepted", id }, 202);
      }

      // ── Services ──
      if (request.method === "GET" && path === "/api/v1/monitor/services") {
        return json({ services: db.getAllServices() }, 200);
      }

      const serviceMatch = path.match(/^\/api\/v1\/monitor\/services\/([^/]+)$/);
      if (request.method === "GET" && serviceMatch) {
        const name = serviceMatch[1]!;
        const svc = db.getService(name);
        if (!svc) return json({ error: "service not found" }, 404);
        return json({ service: svc }, 200);
      }

      // ── Metrics ──
      if (request.method === "GET" && path === "/api/v1/monitor/metrics") {
        const name = url.searchParams.get("name") ?? undefined;
        const source = url.searchParams.get("source") ?? undefined;
        const limit = Number(url.searchParams.get("limit") || "100");
        const opts: { name?: string; source?: string; limit?: number } = { limit };
        if (name) opts.name = name;
        if (source) opts.source = source;
        return json(db.queryMetrics(opts), 200);
      }

      const statsMatch = path.match(/^\/api\/v1\/monitor\/metrics\/([^/]+)\/stats$/);
      if (request.method === "GET" && statsMatch) {
        const name = statsMatch[1]!;
        const window = Number(url.searchParams.get("window") || "300");
        const stats = db.getMetricStats(name, window, new Date().toISOString());
        return json(stats, 200);
      }

      // ── Alert Rules ──
      if (request.method === "GET" && path === "/api/v1/monitor/alerts/rules") {
        return json({ rules: db.getAllAlertRules() }, 200);
      }

      if (request.method === "POST" && path === "/api/v1/monitor/alerts/rules") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const metric = typeof body.metric === "string" ? body.metric : "";
        if (!metric) return json({ error: "metric is required" }, 400);

        const rule: AlertRule = {
          id: randomUUID(),
          metric,
          condition: (typeof body.condition === "string" ? body.condition : "gt") as AlertRule["condition"],
          threshold: typeof body.threshold === "number" ? body.threshold : 0,
          tags: (typeof body.tags === "object" ? body.tags : {}) as Record<string, string>,
          severity: (typeof body.severity === "string" ? body.severity : "info") as AlertRule["severity"],
          description: typeof body.description === "string" ? body.description : "",
          cooldownSeconds: typeof body.cooldownSeconds === "number" ? body.cooldownSeconds : 300,
        };
        db.insertAlertRule(rule);
        return json({ rule }, 201);
      }

      // ── Active Alerts ──
      if (request.method === "GET" && path === "/api/v1/monitor/alerts") {
        return json({ alerts: db.getAllActiveAlerts() }, 200);
      }

      const ackMatch = path.match(/^\/api\/v1\/monitor\/alerts\/([^/]+)\/acknowledge$/);
      if (request.method === "POST" && ackMatch) {
        const ruleId = ackMatch[1]!;
        db.acknowledgeAlert(ruleId);
        return json({ acknowledged: true, ruleId }, 200);
      }

      // ── Alert History ──
      if (request.method === "GET" && path === "/api/v1/monitor/alerts/history") {
        const ruleId = url.searchParams.get("ruleId") ?? undefined;
        const severity = url.searchParams.get("severity") ?? undefined;
        const limit = Number(url.searchParams.get("limit") || "50");
        const histOpts: { ruleId?: string; severity?: string; limit?: number } = { limit };
        if (ruleId) histOpts.ruleId = ruleId;
        if (severity) histOpts.severity = severity;
        return json({ history: db.getAlertHistory(histOpts) }, 200);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-monitor] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusMonitorHeartbeat(baseUrl);

  return {
    server,
    close: () => {
  stopNexusHeartbeat();
  stopNexusMonitor();
  nexusClient.stop(); clearInterval(expireTimer); stopHeartbeat(); db.close(); server.stop(); },
  };
}
