import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { InferenceEngine } from "./inference-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3085");
  const baseUrl = process.env.NEXUS_NEXUS_INFERENCE_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new InferenceEngine("data/inference.sqlite");

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-inference", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-inference", status: "ready", capabilities: ["inference", "ai", "supercomputing"], cloudIntegration: { enabled: (process.env["NEXUS_INFERENCE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" } }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/inference/models") return json(engine.listModels(), 200);
      if (request.method === "POST" && path === "/api/v1/inference/models") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.registerModel(b.name, b.version || "1.0", b.type || "generic"), 201);
      }
      const mm = path.match(/^\/api\/v1\/inference\/models\/([^/]+)$/);
      if (request.method === "GET" && mm) { const m = engine.getModel(mm[1]!); return m ? json(m, 200) : json({ error: "not found" }, 404); }
      if (request.method === "POST" && path === "/api/v1/inference/predict") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.modelId || !b.input) return json({ error: "modelId and input required" }, 400);
        return json(engine.runPrediction(b.modelId, b.input), 201);
      }
      if (request.method === "GET" && path === "/api/v1/inference/predictions") {
        const modelId = url.searchParams.get("modelId") || undefined;
        return json(engine.listPredictions(modelId), 200);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-inference] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); server.stop(); } };
}
