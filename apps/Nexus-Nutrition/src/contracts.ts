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
    id: "nexus-nutrition",
    name: "Nexus-Nutrition",
    description: "Meal planning, recipe management, and nutrition tracking with AI suggestions",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["nutrition","recipes","ai-suggestions"],
    metadata: {
      version: "v1",
      defaultPort: 3094,
    },
  };
}
