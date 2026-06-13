import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-ide", () => {
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
    expect(body["service"]).toBe("nexus-ide");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-ide");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/ide/projects creates a project", async () => {
    const res = await fetch(`${base}/api/v1/ide/projects`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "MyApp", language: "typescript" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("MyApp");
    expect(body.language).toBe("typescript");
  });

  it("GET /api/v1/ide/projects lists projects", async () => {
    const res = await fetch(`${base}/api/v1/ide/projects`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("POST /api/v1/ide/projects/:id/files creates a file", async () => {
    const prjRes = await fetch(`${base}/api/v1/ide/projects`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "P", language: "ts" }) });
    const prj = await prjRes.json() as any;
    const res = await fetch(`${base}/api/v1/ide/projects/${prj.id}/files`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "index.ts", content: "console.log(1)", language: "typescript" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("index.ts");
  });

  it("PUT /api/v1/ide/files/:id updates file content", async () => {
    const prjRes = await fetch(`${base}/api/v1/ide/projects`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "P2", language: "ts" }) });
    const prj = await prjRes.json() as any;
    const fileRes = await fetch(`${base}/api/v1/ide/projects/${prj.id}/files`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "main.ts", content: "old" }) });
    const file = await fileRes.json() as any;
    const res = await fetch(`${base}/api/v1/ide/files/${file.id}`, { method: "PUT", headers: { "content-type": "application/json" }, body: JSON.stringify({ content: "new content" }) });
    expect(res.status).toBe(200);
    const body = await res.json() as any;
    expect(body.content).toBe("new content");
  });
});
