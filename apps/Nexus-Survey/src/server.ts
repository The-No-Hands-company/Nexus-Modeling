import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { SurveyEngine } from "./survey-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3107"); const baseUrl = process.env.NEXUS_NEXUS_SURVEY_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new SurveyEngine("data/survey.sqlite")
  const phantom = new PhantomApp("nexus-survey");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-survey", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-survey", status: "ready", capabilities: ["surveys","responses","feedback"], cloudIntegration: { enabled: (process.env["NEXUS_SURVEY_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/survey") { const a = url.searchParams.has("active") ? url.searchParams.get("active") === "true" : undefined; return json(engine.listSurveys(a), 200); }
    if (request.method === "POST" && path === "/api/v1/survey") { const b = await request.json().catch(() => ({})) as any; if (!b.title) return json({ error: "title required" }, 400); return json(engine.addSurvey(b.title, b.description || "", b.questions || "[]"), 201); }
    if (request.method === "POST" && path === "/api/v1/survey/responses") { const b = await request.json().catch(() => ({})) as any; if (!b.surveyId || !b.respondentId) return json({ error: "surveyId and respondentId required" }, 400); return json(engine.submitResponse(b.surveyId, b.respondentId, b.answers || "{}", b.score || 0), 201); }
    if (request.method === "GET" && path === "/api/v1/survey/responses") { const sid = url.searchParams.get("surveyId"); if (!sid) return json({ error: "surveyId required" }, 400); return json(engine.getResponses(sid), 200); }
    const svm = path.match(/^\/api\/v1\/survey\/([^/]+)$/); if (request.method === "GET" && svm) { const s = engine.getSurvey(svm[1]!); return s ? json(s, 200) : json({ error: "not found" }, 404); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-survey] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
