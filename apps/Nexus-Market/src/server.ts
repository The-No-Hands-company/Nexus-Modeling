import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { MarketEngine } from "./market-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3088");
  const baseUrl = process.env.NEXUS_NEXUS_MARKET_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new MarketEngine("data/market.sqlite")
  const phantom = new PhantomApp("nexus-market");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-market", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-market", status: "ready", capabilities: ["marketplace", "trading", "federation"], cloudIntegration: { enabled: (process.env["NEXUS_MARKET_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/market/listings") {
        const category = url.searchParams.get("category") || undefined;
        return json(engine.listListings(category), 200);
      }
      if (request.method === "POST" && path === "/api/v1/market/listings") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title || !b.price || !b.seller) return json({ error: "title, price, seller required" }, 400);
        return json(engine.createListing(b.title, b.description || "", b.price, b.category || "", b.seller), 201);
      }
      const lm = path.match(/^\/api\/v1\/market\/listings\/([^/]+)$/);
      if (request.method === "GET" && lm) { const l = engine.getListing(lm[1]!); return l ? json(l, 200) : json({ error: "not found" }, 404); }
      if (request.method === "GET" && path === "/api/v1/market/orders") return json(engine.listOrders(), 200);
      if (request.method === "POST" && path === "/api/v1/market/orders") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.listingId || !b.buyer || !b.amount) return json({ error: "listingId, buyer, amount required" }, 400);
        return json(engine.createOrder(b.listingId, b.buyer, b.amount), 201);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-market] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
