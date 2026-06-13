import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-hr", () => {
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
    expect(body["service"]).toBe("nexus-hr");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-hr");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/hr/departments creates a department", async () => {
    const res = await fetch(`${base}/api/v1/hr/departments`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Engineering" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Engineering");
  });

  it("POST /api/v1/hr/employees creates an employee", async () => {
    await fetch(`${base}/api/v1/hr/departments`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Sales" }) });
    const res = await fetch(`${base}/api/v1/hr/employees`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Alice", email: "alice@test.com", department: "Sales", position: "Manager", salary: 80000 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Alice");
    expect(body.email).toBe("alice@test.com");
  });

  it("GET /api/v1/hr/employees lists employees", async () => {
    const res = await fetch(`${base}/api/v1/hr/employees`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });
});
