import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { SocialEngine } from "./social-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3104"); const baseUrl = process.env.NEXUS_NEXUS_SOCIAL_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new SocialEngine("data/social.sqlite")
  const phantom = new PhantomApp("nexus-social");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-social", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-social", status: "ready", capabilities: ["posts","comments","social"], cloudIntegration: { enabled: (process.env["NEXUS_SOCIAL_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/social/posts") { const t = url.searchParams.get("tag") || undefined; return json(engine.listPosts(t), 200); }
    if (request.method === "POST" && path === "/api/v1/social/posts") { const b = await request.json().catch(() => ({})) as any; if (!b.authorId || !b.content) return json({ error: "authorId and content required" }, 400); return json(engine.addPost(b.authorId, b.content, b.mediaUrl || "", b.tags || ""), 201); }
    const pom = path.match(/^\/api\/v1\/social\/posts\/([^/]+)$/); if (request.method === "GET" && pom) { const po = engine.getPost(pom[1]!); return po ? json(po, 200) : json({ error: "not found" }, 404); }
    if (request.method === "POST" && path === "/api/v1/social/likes") { const b = await request.json().catch(() => ({})) as any; if (!b.postId || !b.userId) return json({ error: "postId and userId required" }, 400); return json(engine.addLike(b.postId, b.userId), 201); }
    if (request.method === "POST" && path === "/api/v1/social/comments") { const b = await request.json().catch(() => ({})) as any; if (!b.postId || !b.authorId || !b.content) return json({ error: "postId, authorId, and content required" }, 400); return json(engine.addComment(b.postId, b.authorId, b.content), 201); }
    if (request.method === "GET" && path === "/api/v1/social/comments") { const pid = url.searchParams.get("postId"); if (!pid) return json({ error: "postId required" }, 400); return json(engine.getComments(pid), 200); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-social] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
