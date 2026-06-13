import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createMonitorServer } from "../src/server";

describe("nexus-monitor", () => {
  let base = "";
  let handle: ReturnType<typeof createMonitorServer>;

  beforeAll(async () => {
    handle = createMonitorServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-monitor");
  });

  it("GET /api/v1/monitor/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/monitor/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(typeof body["eventsReceived"]).toBe("number");
  });

  it("POST /api/v1/monitor/ingest accepts heartbeat", async () => {
    const res = await fetch(`${base}/api/v1/monitor/ingest`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        kind: "heartbeat",
        source: "nexus-auth",
        state: "healthy",
        timestamp: new Date().toISOString(),
      }),
    });
    expect(res.status).toBe(202);
  });

  it("POST /api/v1/monitor/ingest accepts metric and triggers alert", async () => {
    // Create alert rule
    await fetch(`${base}/api/v1/monitor/alerts/rules`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        metric: "cpu_usage",
        condition: "gt",
        threshold: 80,
        severity: "warning",
        description: "CPU usage high",
        cooldownSeconds: 1,
      }),
    });

    // Ingest a metric that triggers the rule
    const res = await fetch(`${base}/api/v1/monitor/ingest`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        kind: "metric",
        source: "nexus-cloud",
        name: "cpu_usage",
        value: 95,
        timestamp: new Date().toISOString(),
      }),
    });
    expect(res.status).toBe(202);

    // Check active alerts
    const alerts = await fetch(`${base}/api/v1/monitor/alerts`);
    expect(alerts.status).toBe(200);
    const alertBody = await alerts.json() as { alerts: Array<{ rule: { metric: string } }> };
    expect(alertBody.alerts.length).toBeGreaterThan(0);
  });

  it("GET /api/v1/monitor/dashboard returns overview", async () => {
    const res = await fetch(`${base}/api/v1/monitor/dashboard`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    const overview = body["overview"] as Record<string, unknown>;
    expect(typeof overview["servicesTotal"]).toBe("number");
    expect(typeof overview["eventsReceived"]).toBe("number");
  });

  it("POST /api/v1/monitor/alerts/:id/acknowledge", async () => {
    const alerts = await fetch(`${base}/api/v1/monitor/alerts`);
    const body = await alerts.json() as { alerts: Array<{ ruleId: string }> };
    if (body.alerts.length > 0) {
      const res = await fetch(`${base}/api/v1/monitor/alerts/${body.alerts[0]!.ruleId}/acknowledge`, {
        method: "POST",
      });
      expect(res.status).toBe(200);
    }
  });
});
