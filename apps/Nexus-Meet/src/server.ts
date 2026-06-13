import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      ...headers,
    },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3090");
  const baseUrl = process.env.NEXUS_NEXUS_MEET_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({
          service: "nexus-meet",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          timestamp: new Date().toISOString(),
        }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/status") {
        return json({
          service: "nexus-meet",
          status: "ready",
          capabilities: ["video-conferencing", "recording", "ai-transcription"],
          cloudIntegration: {
            enabled: (process.env["NEXUS_MEET_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        }, 200);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-meet] Listening on port ${server.port}`);

  const stopHeartbeat = startHeartbeat(baseUrl);

  return {
    server,
    close: () => { stopHeartbeat(); server.stop(); },
  };
}
