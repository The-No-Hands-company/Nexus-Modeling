import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { BookEngine } from "./book-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3067");
  const baseUrl = process.env["NEXUS_NEXUS_BOOK_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new BookEngine("data/book.sqlite")
  const phantom = new PhantomApp("nexus-book");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-book", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-book", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/books") {
        const q = url.searchParams.get("q") || undefined;
        return json({ books: engine.listBooks(q) });
      }
      if (req.method === "POST" && p === "/api/v1/books") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.author) return json({ error: "title and author required" }, 400);
        return json(engine.addBook({ title: b.title as string, author: b.author as string, format: (b.format as string) || "pdf", tags: Array.isArray(b.tags) ? b.tags as string[] : [] }), 201);
      }
      const bookMatch = p.match(/^\/api\/v1\/books\/([^/]+)$/);
      if (req.method === "GET" && bookMatch) {
        const book = engine.getBook(bookMatch[1]!);
        return book ? json(book) : json({ error: "not found" }, 404);
      }
      if (req.method === "POST" && bookMatch && p.endsWith("/read")) { engine.incrementRead(bookMatch[1]!); return json({ read: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-book] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
