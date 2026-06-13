import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string { return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, ""); }
function cloudHeaders(): Record<string, string> {
  return { "content-type": "application/json", ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}) };
}
function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_SEARCH_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}
function cloudIntegrationEnabled(): boolean {
  return (process.env.NEXUS_SEARCH_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerNexusSearchWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST", headers: cloudHeaders(), body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!r.ok) throw new Error(`Nexus-Search registration failed: ${r.status}`);
}
export async function heartbeatNexusSearchWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-search")}/heartbeat`, {
    method: "POST", headers: cloudHeaders(), body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!r.ok) throw new Error(`Nexus-Search heartbeat failed: ${r.status}`);
}
export function startNexusSearchHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) return () => {};
  registerNexusSearchWithCloud(baseUrl).catch((e) => console.warn(`[nexus-search] Cloud registration failed: ${(e as Error).message}`));
  const timer = setInterval(() => {
    heartbeatNexusSearchWithCloud(baseUrl).catch((e) => console.warn(`[nexus-search] Cloud heartbeat failed: ${(e as Error).message}`));
  }, heartbeatIntervalMs());
  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
