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
    authVersion: string;
    supportsServiceTokens: boolean;
    supportsUserManagement: boolean;
    supportsApiKeys: boolean;
    supportsSessions: boolean;
    supportsOAuth: boolean;
    supportsRBAC: boolean;
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-auth",
    name: "Nexus Auth",
    description: "Identity provider, service authentication, API key management, OAuth2/OIDC, RBAC, and session management for the Nexus ecosystem",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "identity",
      "service-auth",
      "token-issuance",
      "token-validation",
      "user-management",
      "api-key-management",
      "session-management",
      "oauth2",
      "oidc",
      "rbac",
    ],
    metadata: {
      authVersion: "v2",
      supportsServiceTokens: true,
      supportsUserManagement: true,
      supportsApiKeys: true,
      supportsSessions: true,
      supportsOAuth: true,
      supportsRBAC: true,
      defaultPort: 4310,
    },
  };
}
