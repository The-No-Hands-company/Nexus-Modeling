import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_MONITOR_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function cloudIntegrationEnabled(): boolean {
  return (process.env.NEXUS_MONITOR_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerNexusMonitorWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST", headers: cloudHeaders(), body: JSON.stringify(payload),
  });
  if (!r.ok) throw new Error(`Nexus-Monitor registration failed: ${r.status}`);
}

export async function heartbeatNexusMonitorWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-monitor")}/heartbeat`, {
    method: "POST", headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!r.ok) throw new Error(`Nexus-Monitor heartbeat failed: ${r.status}`);
}

export function startNexusMonitorHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) return () => {};

  registerNexusMonitorWithCloud(baseUrl).catch((e) => {
    console.warn(`[nexus-monitor] Cloud registration failed: ${(e as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusMonitorWithCloud(baseUrl).catch((e) => {
      console.warn(`[nexus-monitor] Cloud heartbeat failed: ${(e as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
