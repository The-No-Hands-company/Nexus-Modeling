import type { AlertConfig, Alert, AlertLevel, AlertChannel } from "./types";

const alertConfigs = new Map<string, AlertConfig>();
const alertHistory: Alert[] = [];
let alertCounter = 0;
let configCounter = 0;
const maxHistoryLength = 1000;

function generateAlertId(): string {
  return `alert-${Date.now()}-${++alertCounter}`;
}

function generateConfigId(): string {
  return `acfg-${Date.now()}-${++configCounter}`;
}

export function upsertAlertConfig(input: {
  id?: string;
  name: string;
  channel: AlertChannel;
  webhookUrl?: string;
  levels: AlertLevel[];
  eventTypes: string[];
}): AlertConfig {
  const existing = input.id ? alertConfigs.get(input.id) : undefined;

  const config: AlertConfig = {
    id: input.id || generateConfigId(),
    name: input.name,
    channel: input.channel,
    webhookUrl: input.webhookUrl,
    levels: input.levels,
    enabled: existing?.enabled ?? true,
    eventTypes: input.eventTypes,
  };

  alertConfigs.set(config.id, config);
  return config;
}

export function getAlertConfig(id: string): AlertConfig | undefined {
  return alertConfigs.get(id);
}

export function listAlertConfigs(): AlertConfig[] {
  return Array.from(alertConfigs.values());
}

export function deleteAlertConfig(id: string): boolean {
  return alertConfigs.delete(id);
}

export function setAlertConfigEnabled(id: string, enabled: boolean): AlertConfig | undefined {
  const config = alertConfigs.get(id);
  if (!config) return undefined;
  config.enabled = enabled;
  return config;
}

function dispatchWebhook(webhookUrl: string, alert: Alert): Promise<boolean> {
  return fetch(webhookUrl, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      id: alert.id,
      level: alert.level,
      eventType: alert.eventType,
      message: alert.message,
      detail: alert.detail,
      createdAt: alert.createdAt,
      source: "nexus-guardian",
    }),
    signal: AbortSignal.timeout(5000),
  })
    .then((r) => r.ok)
    .catch(() => false);
}

export async function dispatchAlert(input: {
  level: AlertLevel;
  eventType: string;
  message: string;
  detail?: Record<string, unknown>;
}): Promise<Alert> {
  const alert: Alert = {
    id: generateAlertId(),
    configId: "",
    level: input.level,
    eventType: input.eventType,
    message: input.message,
    detail: input.detail,
    createdAt: new Date().toISOString(),
    dispatched: false,
  };

  alertHistory.push(alert);
  if (alertHistory.length > maxHistoryLength) {
    alertHistory.splice(0, alertHistory.length - maxHistoryLength);
  }

  const matchingConfigs = Array.from(alertConfigs.values()).filter(
    (cfg) =>
      cfg.enabled &&
      cfg.levels.includes(input.level) &&
      (cfg.eventTypes.length === 0 || cfg.eventTypes.includes(input.eventType)),
  );

  if (matchingConfigs.length === 0) {
    console.log(`[nexus-guardian] Alert ${alert.id}: ${input.level}/${input.eventType} — ${input.message}`);
    alert.dispatched = true;
    alert.configId = "log-only";
    return alert;
  }

  for (const config of matchingConfigs) {
    alert.configId = config.id;

    if (config.channel === "log" || config.channel === "audit") {
      console.log(`[nexus-guardian] Alert ${alert.id}: [${input.level}] ${input.eventType} — ${input.message}`);
      alert.dispatched = true;
    }

    if (config.channel === "webhook" && config.webhookUrl) {
      const success = await dispatchWebhook(config.webhookUrl, alert);
      alert.dispatched = success;
      if (!success) {
        alert.dispatchError = `Webhook dispatch to ${config.webhookUrl} failed`;
        console.warn(`[nexus-guardian] ${alert.dispatchError}`);
      }
    }
  }

  return alert;
}

export function listAlerts(limit = 50): Alert[] {
  return alertHistory.slice(-limit).reverse();
}

export function getAlert(id: string): Alert | undefined {
  return alertHistory.find((a) => a.id === id);
}

export async function dispatchPolicyViolationAlert(input: {
  scope: string;
  subjectId: string;
  verdict: string;
  reason: string;
}): Promise<void> {
  await dispatchAlert({
    level: "warning",
    eventType: "policy_violation",
    message: `Policy verdict '${input.verdict}' for ${input.scope}:${input.subjectId}`,
    detail: {
      scope: input.scope,
      subjectId: input.subjectId,
      verdict: input.verdict,
      reason: input.reason,
    },
  });
}

export async function dispatchThreatAlert(input: {
  toolId: string;
  severity: string;
  description: string;
}): Promise<void> {
  const level: AlertLevel = input.severity === "critical" ? "critical" : input.severity === "high" ? "error" : "warning";

  await dispatchAlert({
    level,
    eventType: "threat_detected",
    message: `Threat '${input.severity}' detected on ${input.toolId}: ${input.description}`,
    detail: {
      toolId: input.toolId,
      severity: input.severity,
      description: input.description,
    },
  });
}

export async function dispatchHealthAlert(input: {
  toolId: string;
  toolName: string;
  health: string;
  consecutiveFailures: number;
}): Promise<void> {
  const level: AlertLevel = input.health === "offline" ? "error" : "warning";

  await dispatchAlert({
    level,
    eventType: "health_degraded",
    message: `${input.toolName} (${input.toolId}) is ${input.health} after ${input.consecutiveFailures} consecutive failures`,
    detail: {
      toolId: input.toolId,
      toolName: input.toolName,
      health: input.health,
      consecutiveFailures: String(input.consecutiveFailures),
    },
  });
}

export function seedDefaultAlertConfigs(): AlertConfig[] {
  if (alertConfigs.size > 0) return listAlertConfigs();

  return [
    upsertAlertConfig({
      name: "Log all alerts",
      channel: "log",
      levels: ["info", "warning", "error", "critical"],
      eventTypes: [],
    }),
    upsertAlertConfig({
      name: "Audit policy violations",
      channel: "audit",
      levels: ["warning", "error", "critical"],
      eventTypes: ["policy_violation"],
    }),
  ];
}

export function clearAlertHistory(): void {
  alertHistory.length = 0;
  alertCounter = 0;
}

export function clearAlertConfigs(): void {
  alertConfigs.clear();
  alertHistory.length = 0;
  alertCounter = 0;
  configCounter = 0;
}
