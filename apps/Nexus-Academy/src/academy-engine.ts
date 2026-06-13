import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export type CourseLevel = "beginner" | "intermediate" | "advanced";

export interface Course {
  id: string;
  title: string;
  description: string;
  level: CourseLevel;
  category: string;
  durationMinutes: number;
  moduleCount: number;
  enrolledCount: number;
  published: boolean;
  createdAt: string;
  updatedAt: string;
}

export interface Module {
  id: string;
  courseId: string;
  title: string;
  order: number;
  content: string | undefined;
  videoUrl: string | undefined;
  quizId: string | undefined;
  createdAt: string;
}

export interface Enrollment {
  id: string;
  userId: string;
  courseId: string;
  progress: number;
  completedModules: number;
  totalModules: number;
  startedAt: string;
  completedAt: string | undefined;
  certificateIssued: boolean;
}

export interface Quiz {
  id: string;
  moduleId: string;
  title: string;
  questions: QuizQuestion[];
  passingScore: number;
}

export interface QuizQuestion {
  id: string;
  text: string;
  options: string[];
  correctIndex: number;
}

export class AcademyEngine {
  db: Database;

  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec("PRAGMA journal_mode=WAL");
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS courses (
        id TEXT PRIMARY KEY, title TEXT NOT NULL, description TEXT, level TEXT DEFAULT 'beginner',
        category TEXT DEFAULT 'general', duration_minutes INTEGER DEFAULT 0, module_count INTEGER DEFAULT 0,
        enrolled_count INTEGER DEFAULT 0, published INTEGER DEFAULT 0,
        created_at TEXT NOT NULL, updated_at TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS modules (
        id TEXT PRIMARY KEY, course_id TEXT NOT NULL, title TEXT NOT NULL, "order" INTEGER DEFAULT 0,
        content TEXT, video_url TEXT, quiz_id TEXT, created_at TEXT NOT NULL,
        FOREIGN KEY (course_id) REFERENCES courses(id)
      );
      CREATE TABLE IF NOT EXISTS enrollments (
        id TEXT PRIMARY KEY, user_id TEXT NOT NULL, course_id TEXT NOT NULL,
        progress REAL DEFAULT 0, completed_modules INTEGER DEFAULT 0, total_modules INTEGER DEFAULT 0,
        started_at TEXT NOT NULL, completed_at TEXT, certificate_issued INTEGER DEFAULT 0,
        FOREIGN KEY (course_id) REFERENCES courses(id)
      );
      CREATE TABLE IF NOT EXISTS quizzes (
        id TEXT PRIMARY KEY, module_id TEXT NOT NULL, title TEXT NOT NULL,
        questions TEXT NOT NULL DEFAULT '[]', passing_score INTEGER DEFAULT 70,
        FOREIGN KEY (module_id) REFERENCES modules(id)
      );
    `);
  }

  // ── Courses ──
  createCourse(input: { title: string; description?: string; level?: CourseLevel; category?: string }): Course {
    const now = new Date().toISOString();
    const course: Course = {
      id: randomUUID(), title: input.title, description: input.description || "",
      level: input.level || "beginner", category: input.category || "general",
      durationMinutes: 0, moduleCount: 0, enrolledCount: 0,
      published: false, createdAt: now, updatedAt: now,
    };
    this.db.prepare("INSERT INTO courses (id, title, description, level, category, duration_minutes, module_count, enrolled_count, published, created_at, updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?)").run(course.id, course.title, course.description, course.level, course.category, course.durationMinutes, course.moduleCount, course.enrolledCount, 0, course.createdAt, course.updatedAt);
    return course;
  }

  getCourse(id: string): Course | undefined { return this.db.prepare("SELECT * FROM courses WHERE id = ?").get(id) as Course | undefined; }
  listCourses(filter?: { category?: string; level?: string; published?: boolean }): Course[] {
    let q = "SELECT * FROM courses WHERE 1=1";
    const p: unknown[] = [];
    if (filter?.category) { q += " AND category = ?"; p.push(filter.category); }
    if (filter?.level) { q += " AND level = ?"; p.push(filter.level); }
    if (filter?.published !== undefined) { q += " AND published = ?"; p.push(filter.published ? 1 : 0); }
    q += " ORDER BY created_at DESC";
    return this.db.prepare(q).all(...p as any) as Course[];
  }
  deleteCourse(id: string): boolean { return this.db.prepare("DELETE FROM courses WHERE id = ?").run(id).changes > 0; }
  publishCourse(id: string): Course | undefined {
    this.db.prepare("UPDATE courses SET published = 1, updated_at = ? WHERE id = ?").run(new Date().toISOString(), id);
    return this.getCourse(id);
  }

  // ── Modules ──
  addModule(input: { courseId: string; title: string; content?: string; videoUrl?: string; order?: number }): Module {
    const now = new Date().toISOString();
    const mod: Module = { id: randomUUID(), courseId: input.courseId, title: input.title, order: input.order || 0, content: input.content, videoUrl: input.videoUrl, quizId: undefined, createdAt: now };
    this.db.prepare("INSERT INTO modules (id, course_id, title, \"order\", content, video_url, created_at) VALUES (?,?,?,?,?,?,?)").run(mod.id, mod.courseId, mod.title, mod.order, mod.content || null, mod.videoUrl || null, mod.createdAt);
    this.db.prepare("UPDATE courses SET module_count = (SELECT COUNT(*) FROM modules WHERE course_id = ?), updated_at = ? WHERE id = ?").run(mod.courseId, now, mod.courseId);
    return mod;
  }

  getModules(courseId: string): Module[] {
    return this.db.prepare("SELECT * FROM modules WHERE course_id = ? ORDER BY \"order\"").all(courseId) as Module[];
  }

  // ── Enrollments ──
  enroll(userId: string, courseId: string): Enrollment {
    const course = this.getCourse(courseId);
    if (!course) throw new Error("Course not found");

    const existing = this.db.prepare("SELECT * FROM enrollments WHERE user_id = ? AND course_id = ?").get(userId, courseId) as Enrollment | undefined;
    if (existing) return existing;

    const now = new Date().toISOString();
    const enrollment: Enrollment = { id: randomUUID(), userId, courseId, progress: 0, completedModules: 0, totalModules: course.moduleCount, startedAt: now, completedAt: undefined, certificateIssued: false };
    this.db.prepare("INSERT INTO enrollments (id, user_id, course_id, progress, completed_modules, total_modules, started_at, certificate_issued) VALUES (?,?,?,?,?,?,?,?)").run(enrollment.id, enrollment.userId, enrollment.courseId, enrollment.progress, enrollment.completedModules, enrollment.totalModules, enrollment.startedAt, 0);
    this.db.prepare("UPDATE courses SET enrolled_count = enrolled_count + 1 WHERE id = ?").run(courseId);
    return enrollment;
  }

  updateProgress(userId: string, courseId: string, completedModules: number): Enrollment | undefined {
    const enrollment = this.db.prepare("SELECT * FROM enrollments WHERE user_id = ? AND course_id = ?").get(userId, courseId) as Enrollment | undefined;
    if (!enrollment) return undefined;

    const totalModules = Math.max(1, enrollment.totalModules || 1);
    enrollment.completedModules = Math.max(0, Math.min(totalModules, completedModules));
    enrollment.progress = Math.round((enrollment.completedModules / totalModules) * 100);

    if (enrollment.progress >= 100 && !enrollment.completedAt) {
      enrollment.completedAt = new Date().toISOString();
      enrollment.certificateIssued = true;
    }

    this.db.prepare("UPDATE enrollments SET progress = ?, completed_modules = ?, completed_at = ?, certificate_issued = ? WHERE user_id = ? AND course_id = ?").run(enrollment.progress, enrollment.completedModules, enrollment.completedAt || null, enrollment.certificateIssued ? 1 : 0, userId, courseId);
    return enrollment;
  }

  getUserEnrollments(userId: string): (Enrollment & { courseTitle?: string })[] {
    const rows = this.db.prepare("SELECT e.*, c.title as course_title FROM enrollments e JOIN courses c ON e.course_id = c.id WHERE e.user_id = ? ORDER BY e.started_at DESC").all(userId) as Array<Record<string, unknown>>;
    return rows.map((r) => ({ ...r as unknown as Enrollment, courseTitle: r.course_title as string }));
  }

  getCourseProgress(courseId: string): { totalEnrolled: number; completed: number; avgProgress: number } {
    const enrolled = (this.db.prepare("SELECT COUNT(*) as c FROM enrollments WHERE course_id = ?").get(courseId) as { c: number }).c;
    const completed = (this.db.prepare("SELECT COUNT(*) as c FROM enrollments WHERE course_id = ? AND progress >= 100").get(courseId) as { c: number }).c;
    const avg = this.db.prepare("SELECT AVG(progress) as avg FROM enrollments WHERE course_id = ?").get(courseId) as { avg: number } | undefined;
    return { totalEnrolled: enrolled, completed, avgProgress: Math.round(avg?.avg || 0) };
  }

  // ── Quizzes ──
  createQuiz(input: { moduleId: string; title: string; questions: QuizQuestion[]; passingScore?: number }): Quiz {
    const quiz: Quiz = { id: randomUUID(), moduleId: input.moduleId, title: input.title, questions: input.questions, passingScore: input.passingScore || 70 };
    this.db.prepare("INSERT INTO quizzes (id, module_id, title, questions, passing_score) VALUES (?,?,?,?,?)").run(quiz.id, quiz.moduleId, quiz.title, JSON.stringify(quiz.questions), quiz.passingScore);
    this.db.prepare("UPDATE modules SET quiz_id = ? WHERE id = ?").run(quiz.id, quiz.moduleId);
    return quiz;
  }

  getQuiz(id: string): Quiz | undefined {
    const row = this.db.prepare("SELECT * FROM quizzes WHERE id = ?").get(id) as Record<string, unknown> | undefined;
    if (!row) return undefined;
    return { ...row as unknown as Quiz, questions: JSON.parse(row.questions as string) as QuizQuestion[] };
  }

  checkQuizAnswers(quizId: string, answers: number[]): { score: number; passed: boolean; total: number; correct: number } {
    const quiz = this.getQuiz(quizId);
    if (!quiz) return { score: 0, passed: false, total: 0, correct: 0 };

    let correct = 0;
    for (let i = 0; i < quiz.questions.length; i++) {
      if (answers[i] === quiz.questions[i]!.correctIndex) correct++;
    }
    const score = Math.round((correct / quiz.questions.length) * 100);
    return { score, passed: score >= quiz.passingScore, total: quiz.questions.length, correct };
  }

  close(): void { this.db.close(); }
}
