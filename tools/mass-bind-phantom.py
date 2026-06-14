#!/usr/bin/env python3
"""Regenerate all Bun app servers with Phantom + Discovery bindings."""
import re
from pathlib import Path

APPS = Path("/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/apps")
SKIP = {"Nexus-Cloud", "Nexus-Graphic", "Nexus-Photos", "Nexus-Design", "Nexus-Modeling"}

SERVER_TEMPLATE = """import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { %(ENGINE)s } from "./%(DOMAIN_FILE)s-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "%(PORT)s");
  const baseUrl = process.env.%(ENV_PREFIX)s_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new %(ENGINE)s("data/%(SERVICE)s.sqlite");

  const phantom = new PhantomApp("%(SERVICE)s");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "%(SERVICE)s", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), phantom: phantom.status() });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "%(SERVICE)s", status: "ready", capabilities: %(CAPS)s, cloudIntegration: { enabled: (process.env["%(ENV_PREFIX)s_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() });
      if (req.method === "GET" && p === "/api/v1/%(DOMAIN)s/%(DOMAIN_PLURAL)s") return json(engine.list%(METHOD)s());
      if (req.method === "POST" && p === "/api/v1/%(DOMAIN)s/%(DOMAIN_PLURAL)s") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.create%(METHOD)s(b.name), 201); }
      const m = p.match(/^\\/api\\/v1\\/%(DOMAIN)s\\/([^/]+)$/); if (req.method === "GET" && m) { const e = engine.get%(METHOD)s(m[1]!); return e ? json(e) : json({ error: "not found" }, 404); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[%(SERVICE)s] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, phantom, discovery, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
"""

TEST_TEMPLATE = """import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("%(SERVICE)s", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("%(SERVICE)s");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("%(SERVICE)s");
    expect(Array.isArray(body["capabilities"])).toBe(true);
    expect(body["phantom"]).toBeDefined();
    expect((body["phantom"] as any)["bound"]).toBe(true);
  });
});
"""

INDEX_TEMPLATE = """import { createServer } from "./server";

const { close } = await createServer();

process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
"""


def pluralize(s):
    if s.endswith(("s", "x", "z", "ch", "sh")): return s + "es"
    if s.endswith("y") and len(s) > 1 and s[-2] not in "aeiou": return s[:-1] + "ies"
    return s + "s"

def derive_params(content, app_name):
    svc = re.search(r'service:\s*"([^"]+)"', content)
    port = re.search(r'process\.env\.PORT\s*\|\|\s*"(\d+)"', content)
    caps = re.search(r'capabilities:\s*(\[[^\]]+\])', content)
    engine = re.search(r'new (\w+Engine)\(', content)
    domain_match = re.search(r'/api/v1/(\w+)/(\w+)', content)
    if not domain_match:
        # Fallback: try /api/v1/items pattern (Ghost generic template)
        domain_match = re.search(r'/api/v1/(\w+)', content.replace('/api/v1/status', ''))
    
    if not all([svc, port, engine]):
        return None
    
    service = svc.group(1)
    port_str = port.group(1)
    engine_cls = engine.group(1)
    
    # Derive domain from match or from engine class name
    if domain_match:
        domain = domain_match.group(1)
    else:
        # Derive from engine: "AccountEngine" -> "account"
        domain = engine_cls.replace("Engine", "").lower()
    
    # Derive capabilities — use fallback if regex fails
    cap_list = caps.group(1) if caps else '["' + domain + '"]'
    
    # Derive method name (e.g., "TasksEngine" -> "Task", "NotesEngine" -> "Note")
    method = engine_cls.replace("Engine", "")
    
    # ENV prefix: nexus-tasks -> NEXUS_TASKS
    env_prefix = service.replace("nexus-", "").replace("-", "_").upper()
    env_prefix = "NEXUS_" + env_prefix
    
    return {
        "SERVICE": service,
        "PORT": port_str,
        "CAPS": cap_list,
        "ENGINE": engine_cls,
        "DOMAIN": domain,
        "DOMAIN_FILE": domain,  # the -engine.ts file base name
        "DOMAIN_PLURAL": pluralize(domain),
        "METHOD": method,
        "ENV_PREFIX": env_prefix,
    }


count = 0
for app_dir in sorted(APPS.iterdir()):
    if not app_dir.is_dir() or app_dir.name.startswith("."): continue
    if app_dir.name in SKIP: continue
    
    server_ts = app_dir / "src" / "server.ts"
    if not server_ts.exists(): continue
    
    content = server_ts.read_text()
    if "PhantomApp" in content and "phantom: phantom.status()" in content:
        continue  # already bound
    
    # Skip non-standard apps (Rust, Python, custom architectures)
    if "export function createServer" not in content and "export async function createServer" not in content:
        continue
    
    params = derive_params(content, app_dir.name)
    if not params: continue
    
    # Generate fresh server
    new_server = SERVER_TEMPLATE % params
    server_ts.write_text(new_server)
    
    # Update test
    test_ts = app_dir / "tests" / "server.test.ts"
    if test_ts.exists():
        test_ts.write_text(TEST_TEMPLATE % params)
    
    # Update index
    index_ts = app_dir / "src" / "index.ts"
    if index_ts.exists():
        index_ts.write_text(INDEX_TEMPLATE)
    
    count += 1
    print(f"  ✓ {app_dir.name} ({params['SERVICE']})")

print(f"\nRegenerated {count} apps with Phantom + Discovery bindings")
