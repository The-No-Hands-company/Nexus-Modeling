import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { RecipesEngine } from "./recipes-engine";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
    },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3148");
  const baseUrl =
    process.env.NEXUS_RECIPES_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new RecipesEngine("data/nexus-recipes.sqlite");

  const phantom = new PhantomApp("nexus-recipes");
  const phantomId = await phantom.start();

  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-recipes",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-recipes",
            status: "ready",
            capabilities: ["recipes"],
            cloudIntegration: {
              enabled:
                (process.env["NEXUS_RECIPES_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200,
        );

      if (req.method === "GET" && p === "/api/v1/recipes/recipeses")
        return json(engine.listRecipess());

      if (req.method === "POST" && p === "/api/v1/recipes/recipeses") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createRecipes(b.name, b.type), 201);
      }

      const gm = p.match(/^\/api\/v1\/recipes\/recipeses\/([^/]+)$/);
      if (req.method === "GET" && gm) {
        const entity = engine.getRecipes(gm[1]!);
        return entity ? json(entity) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-recipes] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    phantom,
    discovery,
    close: () => { stopHeartbeat(); phantom.stop(); server.stop(); },
  };
}
