import { buildSystemsApiRegistrationPayload } from "./contracts";

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
  const raw = Number(process.env.NEXUS_AUTH_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function enableGuardianIntegration(): boolean {
  const flag = (process.env.NEXUS_AUTH_ENABLE_GUARDIAN_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

function cloudIntegrationEnabled(): boolean {
  const flag = (process.env.NEXUS_AUTH_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

export async function registerNexusAuthWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Auth registration failed with status ${response.status}`);
  }
}

export async function heartbeatNexusAuthWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-auth")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Auth heartbeat failed with status ${response.status}`);
  }
}

export async function validateWithGuardian(
  scope: string,
  subjectId: string,
  context: Record<string, string>,
): Promise<{ allowed: boolean; reason: string }> {
  if (!enableGuardianIntegration()) {
    return { allowed: true, reason: "Guardian integration disabled" };
  }

  try {
    const response = await fetch(`${guardianBaseUrl()}/api/v1/guardian/evaluate`, {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ scope, subjectId, context }),
    });

    if (!response.ok) return { allowed: true, reason: "Guardian unavailable" };

    const data = await response.json() as {
      evaluation?: { verdict: string; reason: string };
    };

    const verdict = data.evaluation?.verdict;
    const denied = verdict === "deny" || verdict === "suspend" || verdict === "quarantine";
    return {
      allowed: !denied,
      reason: data.evaluation?.reason || `Guardian verdict: ${verdict}`,
    };
  } catch {
    return { allowed: true, reason: "Guardian unreachable" };
  }
}

export function startNexusAuthCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  registerNexusAuthWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-auth] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusAuthWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-auth] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}
