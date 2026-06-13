import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-academy", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
    const body = await r.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-academy");
  });

  it("list courses", async () => {
    const r = await fetch(`${base}/api/v1/academy/courses`);
    expect(r.status).toBe(200);
    const body = await r.json() as { courses: Array<{ title: string }> };
    expect(body.courses.length).toBeGreaterThan(0);
    expect(body.courses[0]!.title).toBe("Nexus Fundamentals");
  });

  it("get course details with modules", async () => {
    const courses = await fetch(`${base}/api/v1/academy/courses`);
    const list = await courses.json() as { courses: Array<{ id: string }> };
    const r = await fetch(`${base}/api/v1/academy/courses/${list.courses[0]!.id}`);
    expect(r.status).toBe(200);
    const body = await r.json() as { modules: unknown[]; progress: { totalEnrolled: number } };
    expect(Array.isArray(body.modules)).toBe(true);
    expect(body.modules.length).toBeGreaterThan(0);
  });

  it("enroll and track progress", async () => {
    const courses = await fetch(`${base}/api/v1/academy/courses`);
    const list = await courses.json() as { courses: Array<{ id: string }> };
    const courseId = list.courses[0]!.id;

    const enroll = await fetch(`${base}/api/v1/academy/enroll`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ userId: "user-1", courseId }),
    });
    expect(enroll.status).toBe(201);

    const prog = await fetch(`${base}/api/v1/academy/progress`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ userId: "user-1", courseId, completedModules: 2 }),
    });
    const progData = await prog.json() as { enrollment: { progress: number; certificateIssued: boolean } };
    expect(progData.enrollment.progress).toBe(100);
    expect(progData.enrollment.certificateIssued).toBe(true);
  });
});
