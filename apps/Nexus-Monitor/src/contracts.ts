export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: {
    monitorVersion: string;
    supportsMetricsIngestion: boolean;
    supportsAlerting: boolean;
    supportsServiceTracking: boolean;
    supportsDashboard: boolean;
    databaseEngine: string;
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-monitor",
    name: "Nexus Monitor",
    description: "Ecosystem observability — metrics ingestion, alerting with rules, service heartbeat tracking, and dashboard overview",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "observability",
      "metrics-ingestion",
      "alerting",
      "service-tracking",
      "dashboard",
    ],
    metadata: {
      monitorVersion: "v2",
      supportsMetricsIngestion: true,
      supportsAlerting: true,
      supportsServiceTracking: true,
      supportsDashboard: true,
      databaseEngine: "sqlite",
      defaultPort: 3030,
    },
  };
}
