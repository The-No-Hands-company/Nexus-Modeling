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
    tunnelVersion: string;
    supportsPublicExposure: boolean;
    supportsHealthAwareRouting: boolean;
    requiresGuardianApproval: boolean;
    supportsReachabilityCheck: boolean;
    supportsTlsManagement: boolean;
    supportsExposureLifecycle: boolean;
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-tunnel",
    name: "Nexus Tunnel",
    description: "Secure exposure management, health-aware intelligent routing, reachability validation, and TLS certificate lifecycle — the ecosystem's HTTPS tunnel fabric",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "routing",
      "exposure-control",
      "tunnel",
      "health-aware",
      "guardian-aware",
      "reachability",
      "tls-management",
      "exposure-lifecycle",
    ],
    metadata: {
      tunnelVersion: "v2",
      supportsPublicExposure: true,
      supportsHealthAwareRouting: true,
      requiresGuardianApproval: true,
      supportsReachabilityCheck: true,
      supportsTlsManagement: true,
      supportsExposureLifecycle: true,
      defaultPort: 4330,
    },
  };
}
