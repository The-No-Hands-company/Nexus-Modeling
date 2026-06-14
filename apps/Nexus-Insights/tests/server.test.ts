import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-insights", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-insights");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-insights");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/insights/datasets creates a dataset", async () => {
    const res = await fetch(`${base}/api/v1/insights/datasets`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Sales Data", columns: ["date", "revenue", "units"] }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Sales Data");
  });

  it("POST /api/v1/insights/reports creates a report", async () => {
    const dsRes = await fetch(`${base}/api/v1/insights/datasets`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "DS", columns: ["a", "b"] }) });
    const ds = await dsRes.json() as any;
    const res = await fetch(`${base}/api/v1/insights/reports`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Q1 Summary", type: "bar", dataset: ds.id }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Q1 Summary");
    expect(body.type).toBe("bar");
  });

  it("GET /api/v1/insights/reports lists reports", async () => {
    const res = await fetch(`${base}/api/v1/insights/reports`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });
});
