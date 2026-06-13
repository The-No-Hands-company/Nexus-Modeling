export type SystemsApiRegistrationPayload = {
  id: string; name: string; description: string;
  mode: "orchestrated" | "standalone"; exposed: boolean;
  health: "healthy" | "degraded" | "offline"; upstreamUrl: string;
  capabilities: string[]; metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-team-chat", name: "Nexus Team Chat",
    description: "Real-time team messaging with WebSocket gateway, channels, threads, typing indicators, and presence — Discord-like protocol (Hello → Identify → Ready → Dispatch)",
    mode: "orchestrated", exposed: true, health: "healthy", upstreamUrl: baseUrl,
    capabilities: ["websocket-gateway", "channels", "messaging", "typing-indicators", "presence", "realtime"],
    metadata: { chatVersion: "v1", supportsWebSocket: true, protocol: "nexus-gateway-v1", defaultPort: 3109 },
  };
}
