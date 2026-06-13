import { buildSystemsApiRegistrationPayload } from "./contracts";
import type { FileRecord } from "./storage";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_FILES_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function cloudIntegrationEnabled(): boolean {
  return (process.env.NEXUS_FILES_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerNexusFilesWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });
  if (!response.ok) throw new Error(`Nexus-Files registration failed: ${response.status}`);
}

export async function heartbeatNexusFilesWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-files")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!response.ok) throw new Error(`Nexus-Files heartbeat failed: ${response.status}`);
}

export function startNexusFilesHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) return () => {};

  registerNexusFilesWithCloud(baseUrl).catch((e) => {
    console.warn(`[nexus-files] Cloud registration failed: ${(e as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusFilesWithCloud(baseUrl).catch((e) => {
      console.warn(`[nexus-files] Cloud heartbeat failed: ${(e as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
