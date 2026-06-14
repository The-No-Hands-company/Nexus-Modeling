import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { LearnEngine } from "./learn-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3087");
  const baseUrl = process.env.NEXUS_NEXUS_LEARN_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new LearnEngine("data/learn.sqlite")
  const phantom = new PhantomApp("nexus-learn");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-learn", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-learn", status: "ready", capabilities: ["lms", "courses", "ai-tutoring"], cloudIntegration: { enabled: (process.env["NEXUS_LEARN_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/learn/courses") return json(engine.listCourses(), 200);
      if (request.method === "POST" && path === "/api/v1/learn/courses") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title) return json({ error: "title required" }, 400);
        return json(engine.createCourse(b.title, b.description || ""), 201);
      }
      const cm = path.match(/^\/api\/v1\/learn\/courses\/([^/]+)$/);
      if (request.method === "GET" && cm) { const c = engine.getCourse(cm[1]!); return c ? json(c, 200) : json({ error: "not found" }, 404); }
      const lm = path.match(/^\/api\/v1\/learn\/courses\/([^/]+)\/lessons$/);
      if (request.method === "GET" && lm) return json(engine.listLessons(lm[1]!), 200);
      if (request.method === "POST" && lm) {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title) return json({ error: "title required" }, 400);
        return json(engine.addLesson(lm[1]!, b.title, b.content || "", b.order || 0), 201);
      }
      if (request.method === "POST" && path === "/api/v1/learn/progress") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.lessonId) return json({ error: "lessonId required" }, 400);
        return json(engine.markComplete(b.lessonId, b.score), 201);
      }
      if (request.method === "GET" && path === "/api/v1/learn/progress") return json(engine.getProgress(), 200);

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-learn] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
