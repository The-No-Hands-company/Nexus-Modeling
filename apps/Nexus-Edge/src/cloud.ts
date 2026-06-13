import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function guardianBaseUrl(): string {
  return (process.env.NEXUS_GUARDIAN_URL || "http://localhost:4320").trim().replace(/\/$/, "");
}

function tunnelBaseUrl(): string {
  return (process.env.NEXUS_TUNNEL_URL || "http://localhost:4330").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_EDGE_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function enableGuardianIntegration(): boolean {
  return (process.env.NEXUS_EDGE_ENABLE_GUARDIAN_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

function cloudIntegrationEnabled(): boolean {
  return (process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerNexusEdgeWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });
  if (!response.ok) throw new Error(`Nexus-Edge registration failed with status ${response.status}`);
}

export async function heartbeatNexusEdgeWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-edge")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!response.ok) throw new Error(`Nexus-Edge heartbeat failed with status ${response.status}`);
}

export async function requestGuardianThreatResponse(
  toolId: string,
  severity: string,
  description: string,
): Promise<{ recommended: string; reason?: string }> {
  if (!enableGuardianIntegration()) {
    return { recommended: "alert", reason: "Guardian integration disabled" };
  }

  try {
    const response = await fetch(`${guardianBaseUrl()}/api/v1/guardian/threat/respond`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ toolId, threatType: "security", severity, description }),
    });

    if (response.ok) {
      const data = await response.json() as {
        response?: { recommendedAction: string; reason: string };
      };
      return {
        recommended: data.response?.recommendedAction || "alert",
        reason: data.response?.reason,
      };
    }
  } catch (error) {
    console.warn(`[nexus-edge] Guardian query failed: ${(error as Error).message}`);
  }

  return { recommended: "alert", reason: "Guardian unavailable" };
}

export async function registerRouteWithTunnel(
  host: string,
  upstreamUrl: string,
): Promise<boolean> {
  try {
    const response = await fetch(`${tunnelBaseUrl()}/api/v1/tunnel/routes`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({
        toolId: host,
        targetUrl: upstreamUrl,
        exposureMode: "internal",
      }),
    });
    return response.ok;
  } catch (error) {
    console.warn(`[nexus-edge] Tunnel registration failed: ${(error as Error).message}`);
    return false;
  }
}

export async function pushHealthToGuardian(route: { host: string; upstreamUrl: string }): Promise<void> {
  if (!enableGuardianIntegration()) return;
  try {
    await fetch(`${guardianBaseUrl()}/api/v1/guardian/health/heartbeat`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({
        toolId: route.host,
        toolName: route.host,
        upstreamUrl: route.upstreamUrl,
        health: "healthy",
      }),
    });
  } catch {}
}

export function startNexusEdgeCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) return () => {};

  registerNexusEdgeWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-edge] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusEdgeWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-edge] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
