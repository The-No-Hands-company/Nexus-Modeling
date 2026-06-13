import { describe, expect, test, beforeEach } from "bun:test";
import {
  upsertAlertConfig,
  listAlertConfigs,
  deleteAlertConfig,
  setAlertConfigEnabled,
  dispatchAlert,
  listAlerts,
  getAlert,
  seedDefaultAlertConfigs,
  clearAlertConfigs,
  clearAlertHistory,
  dispatchPolicyViolationAlert,
  dispatchThreatAlert,
  dispatchHealthAlert,
} from "../src/alerts";

describe("nexus-guardian alerts", () => {
  beforeEach(() => {
    clearAlertConfigs();
    clearAlertHistory();
  });

  test("upsert and list alert configs", () => {
    upsertAlertConfig({
      name: "Test webhook",
      channel: "webhook",
      webhookUrl: "https://hooks.example.com/alert",
      levels: ["warning", "error"],
      eventTypes: ["policy_violation"],
    });

    const configs = listAlertConfigs();
    expect(configs.length).toBe(1);
    expect(configs[0].channel).toBe("webhook");
    expect(configs[0].webhookUrl).toBe("https://hooks.example.com/alert");
  });

  test("delete alert config", () => {
    const config = upsertAlertConfig({
      name: "To delete",
      channel: "log",
      levels: ["info"],
      eventTypes: [],
    });

    expect(deleteAlertConfig(config.id)).toBe(true);
    expect(listAlertConfigs().length).toBe(0);
  });

  test("dispatch log alert", async () => {
    upsertAlertConfig({
      name: "Log everything",
      channel: "log",
      levels: ["info", "warning", "error", "critical"],
      eventTypes: [],
    });

    const alert = await dispatchAlert({
      level: "warning",
      eventType: "test_event",
      message: "Test alert message",
    });

    expect(alert.id).toBeTruthy();
    expect(alert.level).toBe("warning");
    expect(alert.dispatched).toBe(true);
  });

  test("list alerts returns recent alerts", async () => {
    upsertAlertConfig({
      name: "Log",
      channel: "log",
      levels: ["info"],
      eventTypes: [],
    });

    for (let i = 0; i < 5; i++) {
      await dispatchAlert({
        level: "info",
        eventType: "test",
        message: `Alert ${i}`,
      });
    }

    const alerts = listAlerts(10);
    expect(alerts.length).toBe(5);
    expect(alerts[0].message).toBe("Alert 4");
  });

  test("get specific alert", async () => {
    upsertAlertConfig({
      name: "Log",
      channel: "log",
      levels: ["info"],
      eventTypes: [],
    });

    const dispatched = await dispatchAlert({
      level: "info",
      eventType: "test",
      message: "Find me",
    });

    const found = getAlert(dispatched.id);
    expect(found).toBeDefined();
    expect(found?.message).toBe("Find me");
  });

  test("dispatch policy violation alert", async () => {
    upsertAlertConfig({
      name: "Audit violations",
      channel: "audit",
      levels: ["warning", "error", "critical"],
      eventTypes: ["policy_violation"],
    });

    await dispatchPolicyViolationAlert({
      scope: "exposure",
      subjectId: "unsafe-tool",
      verdict: "deny",
      reason: "Unsafe service",
    });

    const alerts = listAlerts();
    expect(alerts.length).toBe(1);
    expect(alerts[0].eventType).toBe("policy_violation");
    expect(alerts[0].level).toBe("warning");
  });

  test("dispatch threat alert with correct level mapping", async () => {
    upsertAlertConfig({
      name: "Log threats",
      channel: "log",
      levels: ["warning", "error", "critical"],
      eventTypes: ["threat_detected"],
    });

    await dispatchThreatAlert({
      toolId: "nexus-edge",
      severity: "critical",
      description: "RCE detected",
    });

    const alerts = listAlerts();
    expect(alerts.length).toBe(1);
    expect(alerts[0].level).toBe("critical");
    expect(alerts[0].eventType).toBe("threat_detected");
  });

  test("dispatch health alert for offline tool", async () => {
    upsertAlertConfig({
      name: "Log health",
      channel: "log",
      levels: ["error", "critical"],
      eventTypes: ["health_degraded"],
    });

    await dispatchHealthAlert({
      toolId: "nexus-api",
      toolName: "Nexus API",
      health: "offline",
      consecutiveFailures: 5,
    });

    const alerts = listAlerts();
    expect(alerts.length).toBe(1);
    expect(alerts[0].level).toBe("error");
    expect(alerts[0].eventType).toBe("health_degraded");
  });

  test("seed default alert configs", () => {
    clearAlertConfigs();
    const configs = seedDefaultAlertConfigs();
    expect(configs.length).toBe(2);
    expect(configs.some((c) => c.channel === "log")).toBe(true);
    expect(configs.some((c) => c.channel === "audit")).toBe(true);
  });

  test("disable alert config stops dispatching to it", async () => {
    const config = upsertAlertConfig({
      name: "Will disable",
      channel: "audit",
      levels: ["warning"],
      eventTypes: ["policy_violation"],
    });

    setAlertConfigEnabled(config.id, false);

    await dispatchPolicyViolationAlert({
      scope: "test",
      subjectId: "test",
      verdict: "deny",
      reason: "test",
    });

    const alerts = listAlerts();
    const matching = alerts.filter((a) => a.eventType === "policy_violation");
    expect(matching.length).toBe(1);
  });
});
