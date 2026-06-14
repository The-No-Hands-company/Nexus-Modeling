import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { TutorEngine } from "./tutor-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3111"); const baseUrl = process.env.NEXUS_NEXUS_TUTOR_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new TutorEngine("data/tutor.sqlite")
  const phantom = new PhantomApp("nexus-tutor");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-tutor", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-tutor", status: "ready", capabilities: ["lessons","quizzes","students","tutoring"], cloudIntegration: { enabled: (process.env["NEXUS_TUTOR_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/tutor/lessons") { const subj = url.searchParams.get("subject") || undefined; return json(engine.listLessons(subj), 200); }
    if (request.method === "POST" && path === "/api/v1/tutor/lessons") { const b = await request.json().catch(() => ({})) as any; if (!b.title) return json({ error: "title required" }, 400); return json(engine.addLesson(b.title, b.content || "", b.subject || "", b.difficulty || "beginner", b.duration || 0), 201); }
    const lsm = path.match(/^\/api\/v1\/tutor\/lessons\/([^/]+)$/); if (request.method === "GET" && lsm) { const l = engine.getLesson(lsm[1]!); return l ? json(l, 200) : json({ error: "not found" }, 404); }
    if (request.method === "GET" && path === "/api/v1/tutor/quizzes") { const lid = url.searchParams.get("lessonId"); if (!lid) return json({ error: "lessonId required" }, 400); return json(engine.getQuizzes(lid), 200); }
    if (request.method === "POST" && path === "/api/v1/tutor/quizzes") { const b = await request.json().catch(() => ({})) as any; if (!b.lessonId || !b.question) return json({ error: "lessonId and question required" }, 400); return json(engine.addQuiz(b.lessonId, b.question, b.options || "[]", b.correctAnswer || ""), 201); }
    if (request.method === "GET" && path === "/api/v1/tutor/students") return json(engine.listStudents(), 200);
    if (request.method === "POST" && path === "/api/v1/tutor/students") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.enrollStudent(b.name, b.email || ""), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-tutor] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
