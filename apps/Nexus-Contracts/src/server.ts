import { randomUUID } from "node:crypto";
import { ContractEngine } from "./contract-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3072");
  const baseUrl = process.env["NEXUS_NEXUS_CONTRACTS_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new ContractEngine("data/contracts.sqlite")
  const phantom = new PhantomApp("nexus-contracts");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-contracts", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-contracts", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/contracts") return json({ contracts: engine.listContracts() });
      if (req.method === "POST" && p === "/api/v1/contracts") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.partyA || !b.partyB) return json({ error: "title, partyA, partyB required" }, 400);
        return json(engine.createContract({ title: b.title as string, partyA: b.partyA as string, partyB: b.partyB as string, content: b.content as string, expiresAt: b.expiresAt as string }), 201);
      }
      const cMatch = p.match(/^\/api\/v1\/contracts\/([^/]+)$/);
      if (req.method === "GET" && cMatch) { const c = engine.getContract(cMatch[1]!); return c ? json(c) : json({ error: "not found" }, 404); }
      if (req.method === "POST" && cMatch && p.endsWith("/sign")) {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        const c = engine.signContract(cMatch[1]!, b.party as string);
        return c ? json(c) : json({ error: "not found" }, 404);
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-contracts] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
