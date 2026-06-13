import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Course { id: string; title: string; description: string; lessons: number; createdAt: string; }
export interface Lesson { id: string; courseId: string; title: string; content: string; order: number; createdAt: string; }
export interface Progress { id: string; lessonId: string; completed: boolean; score: number; createdAt: string; }
export class LearnEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS courses (id TEXT PRIMARY KEY, title TEXT, description TEXT, lessons INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS lessons (id TEXT PRIMARY KEY, course_id TEXT, title TEXT, content TEXT, lesson_order INTEGER, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS progress (id TEXT PRIMARY KEY, lesson_id TEXT, completed INTEGER DEFAULT 0, score REAL DEFAULT 0, created_at TEXT)");
  }
  createCourse(title: string, description: string): Course {
    const c: Course = { id: randomUUID(), title, description, lessons: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO courses VALUES (?,?,?,?,?)").run(c.id, c.title, c.description, c.lessons, c.createdAt); return c;
  }
  listCourses(): Course[] { return this.db.prepare("SELECT * FROM courses ORDER BY created_at DESC").all() as Course[]; }
  getCourse(id: string): Course | undefined { return this.db.prepare("SELECT * FROM courses WHERE id = ?").get(id) as Course | undefined; }
  addLesson(courseId: string, title: string, content: string, order: number): Lesson {
    const l: Lesson = { id: randomUUID(), courseId, title, content, order, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO lessons VALUES (?,?,?,?,?,?)").run(l.id, l.courseId, l.title, l.content, l.order, l.createdAt);
    this.db.prepare("UPDATE courses SET lessons = lessons + 1 WHERE id = ?").run(courseId); return l;
  }
  listLessons(courseId: string): Lesson[] {
    return this.db.prepare("SELECT * FROM lessons WHERE course_id = ? ORDER BY lesson_order ASC").all(courseId) as Lesson[];
  }
  markComplete(lessonId: string, score = 100): Progress {
    const p: Progress = { id: randomUUID(), lessonId, completed: true, score, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO progress VALUES (?,?,?,?,?)").run(p.id, p.lessonId, p.completed ? 1 : 0, p.score, p.createdAt); return p;
  }
  getProgress(): Progress[] { return this.db.prepare("SELECT * FROM progress ORDER BY created_at DESC").all() as Progress[]; }
}
