export type ServiceStatus = "healthy" | "degraded" | "unhealthy" | "unknown";

export interface ServiceRecord {
  name: string;
  status: ServiceStatus;
  lastHeartbeat: string;
  registeredAt: string;
  metadata: Record<string, unknown>;
  endpoint: string | undefined;
}

export interface MetricPoint {
  name: string;
  value: number;
  tags: Record<string, string>;
  timestamp: string;
  source: string;
}

export interface MetricStats {
  name: string;
  windowSeconds: number;
  count: number;
  min: number;
  max: number;
  avg: number;
  p95: number;
  latest: number;
}

export interface AlertRule {
  id: string;
  metric: string;
  condition: "gt" | "lt" | "gte" | "lte";
  threshold: number;
  tags: Record<string, string>;
  severity: "info" | "warning" | "error" | "critical";
  description: string;
  cooldownSeconds: number;
}

export interface ActiveAlert {
  ruleId: string;
  firedAt: string;
  lastValue: number;
  acknowledged: boolean;
  rule: AlertRule;
}

export interface AlertHistoryEntry {
  id: number;
  ruleId: string;
  metric: string;
  value: number;
  threshold: number;
  condition: string;
  severity: string;
  description: string;
  firedAt: string;
  source: string;
}

export interface DashboardData {
  timestamp: string;
  uptimeSeconds: number;
  overview: {
    servicesTotal: number;
    servicesHealthy: number;
    servicesDegraded: number;
    servicesUnhealthy: number;
    eventsReceived: number;
    heartbeatsReceived: number;
    metricsStored: number;
    activeAlerts: number;
    alertRulesDefined: number;
  };
  recentAlerts: ActiveAlert[];
  serviceStatuses: Record<string, { status: string; lastHeartbeat: string }>;
}
