import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string { return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, ""); }
function cloudHeaders(): Record<string, string> {
  return { "content-type": "application/json", ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}) };
}
function hbMs(): number { return Math.max(5000, Number(process.env.NEXUS_TEAM_CHAT_CLOUD_HEARTBEAT_INTERVAL_MS || "30000")); }
function enabled(): boolean { return (process.env.NEXUS_TEAM_CHAT_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false"; }

export async function registerWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, { method: "POST", headers: cloudHeaders(), body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)) });
  if (!r.ok) throw new Error(`Nexus-Team-Chat reg failed: ${r.status}`);
}
export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/nexus-team-chat/heartbeat`, { method: "POST", headers: cloudHeaders(), body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }) });
  if (!r.ok) throw new Error(`Nexus-Team-Chat hb failed: ${r.status}`);
}
export function startNexusTeamChatHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};
  registerWithCloud(baseUrl).catch((e) => console.warn(`[nexus-team-chat] Cloud reg failed: ${(e as Error).message}`));
  const t = setInterval(() => { heartbeatWithCloud(baseUrl).catch((e) => console.warn(`[nexus-team-chat] HB failed: ${(e as Error).message}`)); }, hbMs());
  if (typeof t.unref === "function") t.unref();
  return () => clearInterval(t);
}
