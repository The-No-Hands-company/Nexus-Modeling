import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { SalesEngine } from "./sales-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3102"); const baseUrl = process.env.NEXUS_NEXUS_SALES_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new SalesEngine("data/sales.sqlite")
  const phantom = new PhantomApp("nexus-sales");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-sales", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-sales", status: "ready", capabilities: ["products","sales","customers","crm"], cloudIntegration: { enabled: (process.env["NEXUS_SALES_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/sales/products") { const cat = url.searchParams.get("category") || undefined; return json(engine.listProducts(cat), 200); }
    if (request.method === "POST" && path === "/api/v1/sales/products") { const b = await request.json().catch(() => ({})) as any; if (!b.name || !b.sku) return json({ error: "name and sku required" }, 400); return json(engine.addProduct(b.name, b.sku, b.price || 0, b.category || "", b.stock || 0), 201); }
    if (request.method === "GET" && path === "/api/v1/sales/customers") return json(engine.listCustomers(), 200);
    if (request.method === "POST" && path === "/api/v1/sales/customers") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addCustomer(b.name, b.email || "", b.phone || ""), 201); }
    if (request.method === "GET" && path === "/api/v1/sales") { const cid = url.searchParams.get("customerId") || undefined; return json(engine.listSales(cid), 200); }
    if (request.method === "POST" && path === "/api/v1/sales") { const b = await request.json().catch(() => ({})) as any; if (!b.customerId || !b.products) return json({ error: "customerId and products required" }, 400); return json(engine.addSale(b.customerId, b.products, b.total || 0, b.date || new Date().toISOString().split("T")[0]!), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-sales] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
