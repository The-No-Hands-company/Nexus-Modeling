import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-learn", () => {
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
    expect(body["service"]).toBe("nexus-learn");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-learn");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/learn/courses creates a course", async () => {
    const res = await fetch(`${base}/api/v1/learn/courses`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "TypeScript 101", description: "Learn TS" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("TypeScript 101");
  });

  it("POST /api/v1/learn/courses/:id/lessons adds a lesson", async () => {
    const courseRes = await fetch(`${base}/api/v1/learn/courses`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Course", description: "Desc" }) });
    const course = await courseRes.json() as any;
    const res = await fetch(`${base}/api/v1/learn/courses/${course.id}/lessons`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Intro", content: "Hello", order: 1 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Intro");
  });

  it("POST /api/v1/learn/progress marks lesson complete", async () => {
    const courseRes = await fetch(`${base}/api/v1/learn/courses`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "C", description: "D" }) });
    const course = await courseRes.json() as any;
    const lessonRes = await fetch(`${base}/api/v1/learn/courses/${course.id}/lessons`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "L", content: "C", order: 1 }) });
    const lesson = await lessonRes.json() as any;
    const res = await fetch(`${base}/api/v1/learn/progress`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ lessonId: lesson.id, score: 95 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.completed).toBe(true);
    expect(body.score).toBe(95);
  });
});
