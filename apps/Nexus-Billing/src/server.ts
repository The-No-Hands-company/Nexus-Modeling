import { randomUUID } from "node:crypto";
import { BillingEngine } from "./billing-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3066");
  const baseUrl = process.env["NEXUS_NEXUS_BILLING_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new BillingEngine("data/billing.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-billing", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-billing", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/billing/subscriptions") {
        const cid = url.searchParams.get("customerId") || undefined;
        return json({ subscriptions: engine.listSubscriptions(cid), revenue: engine.getRevenue() });
      }
      if (req.method === "POST" && p === "/api/v1/billing/subscriptions") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.customerId || !b.plan || !b.amount) return json({ error: "customerId, plan, amount required" }, 400);
        return json(engine.createSubscription(b.customerId as string, b.plan as string, b.amount as number, (b.interval as any) || "monthly"), 201);
      }
      if (req.method === "POST" && p === "/api/v1/billing/payments") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.subscriptionId || !b.amount) return json({ error: "subscriptionId, amount required" }, 400);
        return json(engine.recordPayment(b.subscriptionId as string, b.amount as number, (b.method as string) || "card"), 201);
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-billing] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
