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
    edgeVersion: string;
    supportsThreatDetection: boolean;
    supportsAnomalyDetection: boolean;
    supportsAIGuardrails: boolean;
    supportsAutoResponse: boolean;
    supportsRateLimiting: boolean;
    supportsTrafficMetrics: boolean;
    supportsHealthChecks: boolean;
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-edge",
    name: "Nexus Edge",
    description: "Gateway orchestration with rate limiting, traffic metrics, health-aware routing, AI behavior guardrails, and real-time threat response",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "threat-detection",
      "edge-enforcement",
      "ai-guardrails",
      "anomaly-detection",
      "auto-response",
      "rate-limiting",
      "traffic-metrics",
      "health-checks",
    ],
    metadata: {
      edgeVersion: "v2",
      supportsThreatDetection: true,
      supportsAnomalyDetection: true,
      supportsAIGuardrails: true,
      supportsAutoResponse: true,
      supportsRateLimiting: true,
      supportsTrafficMetrics: true,
      supportsHealthChecks: true,
      defaultPort: 4340,
    },
  };
}
