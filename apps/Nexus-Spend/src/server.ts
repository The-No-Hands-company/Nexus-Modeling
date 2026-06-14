import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { SpendEngine } from "./spend-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3105"); const baseUrl = process.env.NEXUS_NEXUS_SPEND_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new SpendEngine("data/spend.sqlite")
  const phantom = new PhantomApp("nexus-spend");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-spend", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-spend", status: "ready", capabilities: ["expenses","budgets","finance"], cloudIntegration: { enabled: (process.env["NEXUS_SPEND_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/spend/expenses") { const cat = url.searchParams.get("category") || undefined; return json(engine.listExpenses(cat), 200); }
    if (request.method === "POST" && path === "/api/v1/spend/expenses") { const b = await request.json().catch(() => ({})) as any; if (!b.description || !b.amount) return json({ error: "description and amount required" }, 400); return json(engine.addExpense(b.description, b.amount, b.category || "", b.date || new Date().toISOString().split("T")[0]!, b.vendor || ""), 201); }
    const exm = path.match(/^\/api\/v1\/spend\/expenses\/([^/]+)$/); if (request.method === "GET" && exm) { const e = engine.getExpense(exm[1]!); return e ? json(e, 200) : json({ error: "not found" }, 404); }
    if (request.method === "GET" && path === "/api/v1/spend/budgets") return json(engine.listBudgets(), 200);
    if (request.method === "POST" && path === "/api/v1/spend/budgets") { const b = await request.json().catch(() => ({})) as any; if (!b.name || !b.amount) return json({ error: "name and amount required" }, 400); return json(engine.addBudget(b.name, b.category || "", b.amount, b.period || new Date().toISOString().substring(0, 7)), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-spend] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
