import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-mind", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-mind");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-mind");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/mind/maps creates a mind map", async () => {
    const res = await fetch(`${base}/api/v1/mind/maps`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Project Plan", description: "Q3 roadmap" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Project Plan");
  });

  it("POST /api/v1/mind/maps/:id/nodes adds a node", async () => {
    const mapRes = await fetch(`${base}/api/v1/mind/maps`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Map", description: "Desc" }) });
    const map = await mapRes.json() as any;
    const res = await fetch(`${base}/api/v1/mind/maps/${map.id}/nodes`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ label: "Idea 1", content: "First idea", x: 100, y: 200 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.label).toBe("Idea 1");
  });

  it("GET /api/v1/mind/maps lists maps", async () => {
    const res = await fetch(`${base}/api/v1/mind/maps`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });
});
