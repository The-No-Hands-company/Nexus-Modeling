import { buildSystemsApiRegistrationPayload } from "./contracts";
import { listRoutes, registerRoute, setRouteEnabled } from "./state";

type CloudTool = {
  id: string;
  name?: string;
  upstreamUrl?: string;
  health?: string;
  exposed?: boolean;
  registrationStatus?: string;
};

function cloudBaseUrl(): string {
  const raw = (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim();
  return raw.replace(/\/$/, "");
}

function guardianBaseUrl(): string {
  const raw = (process.env.NEXUS_GUARDIAN_URL || "http://localhost:4320").trim();
  return raw.replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_TUNNEL_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function reconcileIntervalMs(): number {
  const raw = Number(process.env.NEXUS_TUNNEL_CLOUD_RECONCILE_INTERVAL_MS || "15000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 15000;
}

function enableGuardianIntegration(): boolean {
  const flag = (process.env.NEXUS_TUNNEL_ENABLE_GUARDIAN_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

function cloudIntegrationEnabled(): boolean {
  const flag = (process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

export async function registerNexusTunnelWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel registration failed with status ${response.status}`);
  }
}

export async function heartbeatNexusTunnelWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-tunnel")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel heartbeat failed with status ${response.status}`);
  }
}

async function listToolsFromCloud(): Promise<CloudTool[]> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "GET",
    headers: cloudHeaders(),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel reconcile tools failed with status ${response.status}`);
  }

  const data = (await response.json()) as { tools?: CloudTool[] };
  return Array.isArray(data.tools) ? data.tools : [];
}

export async function pushToolAddressesToCloud(
  toolId: string,
  publicUrl: string,
  status: string,
): Promise<boolean> {
  try {
    const response = await fetch(`${cloudBaseUrl()}/api/v1/addresses`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({
        toolId,
        kind: "website",
        desiredHost: publicUrl,
      }),
    });

    if (!response.ok) return false;

    const data = await response.json() as { address?: { id: string } };
    return !!data.address?.id;
  } catch (error) {
    console.warn(`[nexus-tunnel] Failed to push address to Cloud: ${(error as Error).message}`);
    return false;
  }
}

async function evaluateWithGuardian(
  scope: string,
  subjectId: string,
  context: Record<string, string>,
): Promise<{ verdict: string; reason: string; matchedRuleIds: string[] } | null> {
  if (!enableGuardianIntegration()) return null;

  try {
    const response = await fetch(`${guardianBaseUrl()}/api/v1/guardian/evaluate`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ scope, subjectId, context }),
    });

    if (!response.ok) return null;

    const data = await response.json() as {
      evaluation?: { verdict: string; reason: string; matchedRuleIds: string[] };
    };

    return data.evaluation || null;
  } catch (error) {
    console.warn(`[nexus-tunnel] Guardian evaluate failed: ${(error as Error).message}`);
    return null;
  }
}

export async function evaluateToolTrust(toolId: string): Promise<{
  trusted: boolean;
  verdict: string;
  reason: string;
}> {
  const evaluation = await evaluateWithGuardian("exposure", toolId, {
    toolId,
    source: "nexus-tunnel",
  });

  if (!evaluation) {
    return { trusted: true, verdict: "approve", reason: "Guardian unavailable — default approve" };
  }

  const denied = evaluation.verdict === "deny" || evaluation.verdict === "suspend" || evaluation.verdict === "quarantine";
  return {
    trusted: !denied,
    verdict: evaluation.verdict,
    reason: evaluation.reason,
  };
}

function isToolHealthyForRouting(tool: CloudTool): boolean {
  const health = (tool.health || "").toLowerCase();
  const status = (tool.registrationStatus || "").toLowerCase();
  if (status === "offline") return false;
  return health !== "offline" && health !== "degraded";
}

export async function reconcileNexusTunnelRoutesFromCloud(): Promise<void> {
  const tools = await listToolsFromCloud();
  const cloudToolIds = new Set<string>();

  for (const tool of tools) {
    if (!tool?.id || tool.id === "nexus-tunnel") continue;
    cloudToolIds.add(tool.id);

    if (!tool.upstreamUrl) {
      setRouteEnabled(tool.id, false);
      continue;
    }

    const exposureMode = tool.exposed ? "public" : "internal";
    registerRoute(tool.id, tool.upstreamUrl, exposureMode);

    const healthyForRouting = isToolHealthyForRouting(tool);
    const trustResult = await evaluateToolTrust(tool.id);
    setRouteEnabled(tool.id, healthyForRouting && trustResult.trusted);
  }

  for (const route of listRoutes()) {
    if (!cloudToolIds.has(route.toolId)) {
      setRouteEnabled(route.toolId, false);
    }
  }
}

export async function requestGuardianApprovalForExposure(
  toolId: string,
  exposureMode: string,
): Promise<{ approved: boolean; reason?: string }> {
  const evaluation = await evaluateWithGuardian("exposure", toolId, {
    toolId,
    exposureMode,
    source: "nexus-tunnel",
    requestType: "exposure-approval",
  });

  if (!evaluation) {
    return { approved: false, reason: "Guardian unavailable — denying for security" };
  }

  return {
    approved: evaluation.verdict === "approve",
    reason: evaluation.reason || `Guardian verdict: ${evaluation.verdict}`,
  };
}

export async function pushHealthToGuardian(
  toolId: string,
  toolName: string,
  upstreamUrl: string,
  health: string,
): Promise<void> {
  if (!enableGuardianIntegration()) return;

  try {
    await fetch(`${guardianBaseUrl()}/api/v1/guardian/health/heartbeat`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ toolId, toolName, upstreamUrl, health }),
    });
  } catch (error) {
    console.warn(`[nexus-tunnel] Failed to push health to Guardian: ${(error as Error).message}`);
  }
}

export async function checkGuardianRateLimit(
  toolId: string,
  scope: string,
): Promise<{ throttled: boolean; remaining: number } | null> {
  if (!enableGuardianIntegration()) return null;

  try {
    const response = await fetch(`${guardianBaseUrl()}/api/v1/guardian/rate-limits/${encodeURIComponent(toolId)}`, {
      method: "GET",
      headers: cloudHeaders(),
    });

    if (!response.ok) return null;

    const data = await response.json() as {
      rateLimits?: Array<{ scope: string; throttled: boolean; remaining: number }>;
    };

    const matched = (data.rateLimits || []).find((r) => r.scope === scope);
    return matched ? { throttled: matched.throttled, remaining: matched.remaining } : null;
  } catch {
    return null;
  }
}

export function startNexusTunnelCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  registerNexusTunnelWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-tunnel] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusTunnelWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-tunnel] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}

export function startNexusTunnelCloudReconciliationLoop(): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  reconcileNexusTunnelRoutesFromCloud().catch((error) => {
    console.warn(`[nexus-tunnel] Cloud reconcile failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    reconcileNexusTunnelRoutesFromCloud().catch((error) => {
      console.warn(`[nexus-tunnel] Cloud reconcile failed: ${(error as Error).message}`);
    });
  }, reconcileIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}
