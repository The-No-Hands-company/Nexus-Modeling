import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { CrmEngine } from "./crm-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3073");
  const baseUrl = process.env["NEXUS_NEXUS_CRM_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new CrmEngine("data/crm.sqlite")
  const phantom = new PhantomApp("nexus-crm");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-crm", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-crm", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/crm/contacts") return json({ contacts: engine.listContacts() });
      if (req.method === "POST" && p === "/api/v1/crm/contacts") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createContact({ name: b.name as string, email: b.email as string, phone: b.phone as string, company: b.company as string, tags: Array.isArray(b.tags) ? b.tags as string[] : [] }), 201);
      }
      if (req.method === "GET" && p === "/api/v1/crm/deals") return json({ deals: engine.listDeals() });
      if (req.method === "POST" && p === "/api/v1/crm/deals") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.contactId || !b.value) return json({ error: "title, contactId, value required" }, 400);
        return json(engine.createDeal({ title: b.title as string, contactId: b.contactId as string, value: b.value as number, stage: (b.stage as string) || "lead", probability: b.probability as number }), 201);
      }
      if (req.method === "GET" && p === "/api/v1/crm/pipeline") return json({ pipeline: engine.getPipeline() });
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-crm] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
