import { randomUUID } from "node:crypto";
import { AcademyEngine } from "./academy-engine";
import { startNexusAcademyHeartbeat } from "./cloud";

function json(payload: unknown, status = 200): Response {
  return new Response(JSON.stringify(payload), { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3060");
  const baseUrl = process.env.NEXUS_ACADEMY_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new AcademyEngine(process.env.NEXUS_ACADEMY_DB_PATH || "data/academy.sqlite");

  if (engine.listCourses().length === 0) {
    const c = engine.createCourse({ title: "Nexus Fundamentals", description: "Learn the core concepts of the Nexus ecosystem", level: "beginner", category: "platform" });
    engine.addModule({ courseId: c.id, title: "Welcome to Nexus", content: "Introduction to the sovereign ecosystem", order: 1 });
    engine.addModule({ courseId: c.id, title: "Setting Up", content: "Installation and configuration", order: 2 });
    engine.publishCourse(c.id);
  }

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health") {
        return json({ service: "nexus-academy", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      }
      if (req.method === "GET" && p === "/api/v1/academy/status") {
        return json({ courses: engine.listCourses().length, enrollments: engine.getCourseProgress("any").totalEnrolled });
      }

      // Courses
      if (req.method === "GET" && p === "/api/v1/academy/courses") {
        const level = url.searchParams.get("level") || undefined;
        const category = url.searchParams.get("category") || undefined;
        const opts: { level?: string; category?: string; published?: boolean } = { published: true };
        if (level) opts.level = level;
        if (category) opts.category = category;
        return json({ courses: engine.listCourses(opts) });
      }
      if (req.method === "POST" && p === "/api/v1/academy/courses") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.title) return json({ error: "title required" }, 400);
        const course = engine.createCourse({ title: b.title as string, description: b.description as string, level: (b.level as any) || "beginner", category: b.category as string });
        return json({ course }, 201);
      }
      const courseMatch = p.match(/^\/api\/v1\/academy\/courses\/([^/]+)$/);
      if (req.method === "GET" && courseMatch) {
        const c = engine.getCourse(courseMatch[1]!);
        return c ? json({ course: c, modules: engine.getModules(c.id), progress: engine.getCourseProgress(c.id) }) : json({ error: "not found" }, 404);
      }
      if (req.method === "POST" && courseMatch && p.endsWith("/publish")) {
        const c = engine.publishCourse(courseMatch[1]!);
        return c ? json({ course: c }) : json({ error: "not found" }, 404);
      }
      if (req.method === "DELETE" && courseMatch) {
        return engine.deleteCourse(courseMatch[1]!) ? json({ deleted: true }) : json({ error: "not found" }, 404);
      }

      // Modules
      const modulesMatch = p.match(/^\/api\/v1\/academy\/courses\/([^/]+)\/modules$/);
      if (req.method === "POST" && modulesMatch) {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.title) return json({ error: "title required" }, 400);
        const mod = engine.addModule({ courseId: modulesMatch[1]!, title: b.title as string, content: b.content as string, videoUrl: b.videoUrl as string, order: b.order as number });
        return json({ module: mod }, 201);
      }

      // Enrollments
      if (req.method === "POST" && p === "/api/v1/academy/enroll") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.userId || !b.courseId) return json({ error: "userId and courseId required" }, 400);
        try {
          return json({ enrollment: engine.enroll(b.userId as string, b.courseId as string) }, 201);
        } catch { return json({ error: "course not found" }, 404); }
      }
      if (req.method === "GET" && p === "/api/v1/academy/enrollments") {
        const userId = url.searchParams.get("userId");
        if (!userId) return json({ error: "userId query param required" }, 400);
        return json({ enrollments: engine.getUserEnrollments(userId) });
      }
      if (req.method === "POST" && p === "/api/v1/academy/progress") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.userId || !b.courseId || b.completedModules === undefined) return json({ error: "userId, courseId, completedModules required" }, 400);
        const e = engine.updateProgress(b.userId as string, b.courseId as string, b.completedModules as number);
        return e ? json({ enrollment: e }) : json({ error: "enrollment not found" }, 404);
      }

      // Quizzes
      if (req.method === "POST" && p === "/api/v1/academy/quizzes") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.moduleId || !b.title || !Array.isArray(b.questions)) return json({ error: "moduleId, title, questions required" }, 400);
        return json({ quiz: engine.createQuiz({ moduleId: b.moduleId as string, title: b.title as string, questions: b.questions as any[], passingScore: b.passingScore as number }) }, 201);
      }
      const quizMatch = p.match(/^\/api\/v1\/academy\/quizzes\/([^/]+)$/);
      if (req.method === "GET" && quizMatch) {
        const q = engine.getQuiz(quizMatch[1]!);
        return q ? json({ quiz: q }) : json({ error: "not found" }, 404);
      }
      if (req.method === "POST" && quizMatch && p.endsWith("/submit")) {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!Array.isArray(b.answers)) return json({ error: "answers array required" }, 400);
        return json(engine.checkQuizAnswers(quizMatch[1]!, b.answers as number[]));
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-academy] Listening on port ${server.port}`);
  const stop = startNexusAcademyHeartbeat(baseUrl);
  return { server, close: () => { stop(); engine.close(); server.stop(); } };
}
