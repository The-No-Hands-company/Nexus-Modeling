import { buildSystemsApiRegistrationPayload } from "./contracts";
function cb(): string { return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, ""); }
function ch(): Record<string, string> { return { "content-type": "application/json", ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}) }; }
function hb(): number { return Math.max(5000, Number(process.env.NEXUS_ACADEMY_CLOUD_HEARTBEAT_INTERVAL_MS || "30000")); }
function en(): boolean { return (process.env.NEXUS_ACADEMY_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false"; }
export async function registerWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cb()}/api/v1/tools`, { method: "POST", headers: ch(), body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)) });
  if (!r.ok) throw new Error(`reg failed: ${r.status}`);
}
export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  await fetch(`${cb()}/api/v1/tools/nexus-academy/heartbeat`, { method: "POST", headers: ch(), body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }) });
}
export function startNexusAcademyHeartbeat(baseUrl: string): () => void {
  if (!en()) return () => {};
  registerWithCloud(baseUrl).catch((e) => {});
  const t = setInterval(() => { heartbeatWithCloud(baseUrl).catch(() => {}); }, hb());
  if (typeof t.unref === "function") t.unref();
  return () => clearInterval(t);
}
