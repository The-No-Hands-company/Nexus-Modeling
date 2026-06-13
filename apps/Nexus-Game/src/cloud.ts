import { buildSystemsApiRegistrationPayload } from "./contracts";

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

function hbMs(): number {
  return Math.max(5000, Number(process.env["NEXUS_GAME_CLOUD_HEARTBEAT_INTERVAL_MS"] || "30000"));
}

function enabled(): boolean {
  return (process.env["NEXUS_GAME_ENABLE_CLOUD_INTEGRATION"] || "true").trim().toLowerCase() !== "false";
}

export async function registerWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!r.ok) throw new Error(`Nexus-Game registration failed: ${r.status}`);
}

export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-game")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!r.ok) throw new Error(`Nexus-Game heartbeat failed: ${r.status}`);
}

export function startHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};

  registerWithCloud(baseUrl).catch((e) => {
    console.warn(`[nexus-game] Cloud registration failed: ${(e as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatWithCloud(baseUrl).catch((e) => {
      console.warn(`[nexus-game] Cloud heartbeat failed: ${(e as Error).message}`);
    });
  }, hbMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
