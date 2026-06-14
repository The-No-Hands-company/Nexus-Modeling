import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { OfficeEngine } from "./office-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3095"); const baseUrl = process.env.NEXUS_NEXUS_OFFICE_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new OfficeEngine("data/office.sqlite")
  const phantom = new PhantomApp("nexus-office");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-office", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-office", status: "ready", capabilities: ["documents","templates","office"], cloudIntegration: { enabled: (process.env["NEXUS_OFFICE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/office/documents") { const t = url.searchParams.get("type") || undefined; return json(engine.listDocuments(t), 200); }
    if (request.method === "POST" && path === "/api/v1/office/documents") { const b = await request.json().catch(() => ({})) as any; if (!b.title) return json({ error: "title required" }, 400); return json(engine.addDocument(b.title, b.content || "", b.type || "document", b.tags || ""), 201); }
    const dm = path.match(/^\/api\/v1\/office\/documents\/([^/]+)$/); if (request.method === "GET" && dm) { const d = engine.getDocument(dm[1]!); return d ? json(d, 200) : json({ error: "not found" }, 404); }
    if (request.method === "GET" && path === "/api/v1/office/templates") { const cat = url.searchParams.get("category") || undefined; return json(engine.listTemplates(cat), 200); }
    if (request.method === "POST" && path === "/api/v1/office/templates") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addTemplate(b.name, b.content || "", b.category || ""), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-office] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
