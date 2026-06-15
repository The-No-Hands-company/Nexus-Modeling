export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(
  baseUrl: string,
): SystemsApiRegistrationPayload {
  return {
    id: "nexus-schedule",
    name: "Nexus-Schedule",
    description: "Schedule",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["schedule"],
    metadata: {
      version: "v1",
      defaultPort: 3152,
    },
  };
}
