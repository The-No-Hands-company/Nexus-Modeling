import { randomUUID } from "node:crypto";
import { ContentEngine } from "./content-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3071");
  const baseUrl = process.env["NEXUS_NEXUS_CONTENT_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new ContentEngine("data/content.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-content", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-content", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/content") {
        const type = url.searchParams.get("type") || undefined;
        return json({ items: engine.listContent(type) });
      }
      if (req.method === "POST" && p === "/api/v1/content") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.type) return json({ error: "title and type required" }, 400);
        return json(engine.createContent({ title: b.title as string, type: b.type as string, body: b.body as string, tags: Array.isArray(b.tags) ? b.tags as string[] : [], status: b.status as string }), 201);
      }
      const cMatch = p.match(/^\/api\/v1\/content\/([^/]+)$/);
      if (req.method === "GET" && cMatch) { const c = engine.getContent(cMatch[1]!); return c ? json(c) : json({ error: "not found" }, 404); }
      if (req.method === "POST" && cMatch && p.endsWith("/publish")) { engine.publishContent(cMatch[1]!); return json({ published: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-content] Listening on port ${server.port}`);
  return { server, close: () => { server.stop(); } };
}
