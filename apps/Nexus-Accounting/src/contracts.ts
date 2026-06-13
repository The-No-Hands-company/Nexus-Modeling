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

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-accounting",
    name: "Nexus-Accounting",
    description: "Bookkeeping and invoicing with AI reconciliation and financial reporting",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["accounting","invoicing","ai-reconciliation"],
    metadata: {
      version: "v1",
      defaultPort: 3061,
    },
  };
}
