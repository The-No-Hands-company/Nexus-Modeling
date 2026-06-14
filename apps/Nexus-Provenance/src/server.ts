import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { ProvenanceEngine } from "./provenance-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3100"); const baseUrl = process.env.NEXUS_NEXUS_PROVENANCE_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new ProvenanceEngine("data/provenance.sqlite")
  const phantom = new PhantomApp("nexus-provenance");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-provenance", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-provenance", status: "ready", capabilities: ["provenance","chains","audit"], cloudIntegration: { enabled: (process.env["NEXUS_PROVENANCE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/provenance/records") { const eid = url.searchParams.get("entityId") || undefined; const cid = url.searchParams.get("chainId") || undefined; return json(engine.listRecords(eid, cid), 200); }
    if (request.method === "POST" && path === "/api/v1/provenance/records") { const b = await request.json().catch(() => ({})) as any; if (!b.entityId || !b.action) return json({ error: "entityId and action required" }, 400); return json(engine.addRecord(b.entityId, b.entityType || "", b.action, b.actor || "", b.details || "", b.chainId || ""), 201); }
    if (request.method === "GET" && path === "/api/v1/provenance/chains") return json(engine.listChains(), 200);
    if (request.method === "POST" && path === "/api/v1/provenance/chains") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addChain(b.name, b.description), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-provenance] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
