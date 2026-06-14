import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { CodeReviewEngine } from "./review-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3069");
  const baseUrl = process.env["NEXUS_NEXUS_CODE_REVIEW_BASE_URL"] || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new CodeReviewEngine("data/review.sqlite")
  const phantom = new PhantomApp("nexus-code-review");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-code-review", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-code-review", status: "ready" });
      
      if (req.method === "GET" && p === "/api/v1/reviews") return json({ reviews: engine.listReviews() });
      if (req.method === "POST" && p === "/api/v1/reviews") {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        if (!b.title || !b.code || !b.authorId) return json({ error: "title, code, authorId required" }, 400);
        return json(engine.submitReview({ title: b.title as string, code: b.code as string, language: b.language as string, authorId: b.authorId as string }), 201);
      }
      const rMatch = p.match(/^\/api\/v1\/reviews\/([^/]+)$/);
      if (req.method === "GET" && rMatch) { const r = engine.getReview(rMatch[1]!); return r ? json(r) : json({ error: "not found" }, 404); }
      if (req.method === "POST" && rMatch && p.endsWith("/feedback")) {
        const b = await req.json().catch(() => ({})) as Record<string,unknown>;
        const r = engine.addFeedback(rMatch[1]!, { reviewerId: b.reviewerId as string, verdict: b.verdict as string, comments: b.comments as string, score: b.score as number });
        return r ? json(r) : json({ error: "not found" }, 404);
      }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-code-review] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
