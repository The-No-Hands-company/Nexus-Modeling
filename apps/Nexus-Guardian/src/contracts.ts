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
    guardianVersion: string;
    supportsPolicyEnforcement: boolean;
    supportsThreatDetection: boolean;
    supportsAuditTrail: boolean;
    supportsRuleEngine: boolean;
    supportsRateLimiting: boolean;
    supportsHealthMonitoring: boolean;
    supportsAlertDispatch: boolean;
    supportedScopes: string[];
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-guardian",
    name: "Nexus Guardian",
    description: "Central safety, security, and system health command center — policy enforcement, threat detection, rule engine, rate limiting, health monitoring, and alert dispatch",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "policy-enforcement",
      "threat-detection",
      "threat-response",
      "guardian",
      "audit-trail",
      "rule-engine",
      "rate-limiting",
      "health-monitoring",
      "alert-dispatch",
    ],
    metadata: {
      guardianVersion: "v2",
      supportsPolicyEnforcement: true,
      supportsThreatDetection: true,
      supportsAuditTrail: true,
      supportsRuleEngine: true,
      supportsRateLimiting: true,
      supportsHealthMonitoring: true,
      supportsAlertDispatch: true,
      supportedScopes: [
        "service", "user", "agent", "network", "resource",
        "exposure", "domain", "runtime",
      ],
      defaultPort: 4320,
    },
  };
}
