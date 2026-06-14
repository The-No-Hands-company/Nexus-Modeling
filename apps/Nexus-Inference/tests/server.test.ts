import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-inference", () => {
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
    expect(body["service"]).toBe("nexus-inference");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-inference");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/inference/models register a model", async () => {
    const res = await fetch(`${base}/api/v1/inference/models`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "gpt-4", version: "1.0", type: "llm" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("gpt-4");
    expect(body.type).toBe("llm");
  });

  it("GET /api/v1/inference/models lists models", async () => {
    const res = await fetch(`${base}/api/v1/inference/models`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("POST /api/v1/inference/predict runs prediction", async () => {
    const modelRes = await fetch(`${base}/api/v1/inference/models`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "test", type: "generic" }) });
    const model = await modelRes.json() as any;
    const res = await fetch(`${base}/api/v1/inference/predict`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ modelId: model.id, input: "hello world" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.modelId).toBe(model.id);
    expect(body.output).toBeTruthy();
  });
});
