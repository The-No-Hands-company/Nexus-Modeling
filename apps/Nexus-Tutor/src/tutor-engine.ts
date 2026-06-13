import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Lesson { id: string; title: string; content: string; subject: string; difficulty: string; duration: number; createdAt: string; }
export interface Quiz { id: string; lessonId: string; question: string; options: string; correctAnswer: string; createdAt: string; }
export interface Student { id: string; name: string; email: string; progress: string; enrolledAt: string; }
export class TutorEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS lessons (id TEXT PRIMARY KEY, title TEXT, content TEXT, subject TEXT, difficulty TEXT DEFAULT 'beginner', duration INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS quizzes (id TEXT PRIMARY KEY, lesson_id TEXT, question TEXT, options TEXT, correct_answer TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS students (id TEXT PRIMARY KEY, name TEXT, email TEXT, progress TEXT DEFAULT '', enrolled_at TEXT)"); }
  addLesson(title: string, content: string, subject: string, difficulty: string, duration: number): Lesson {
    const l: Lesson = { id: randomUUID(), title, content, subject, difficulty, duration, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO lessons VALUES (?,?,?,?,?,?,?)").run(l.id, l.title, l.content, l.subject, l.difficulty, l.duration, l.createdAt); return l; }
  listLessons(subject?: string): Lesson[] {
    if (subject) return this.db.prepare("SELECT * FROM lessons WHERE subject = ? ORDER BY title ASC").all(subject) as Lesson[];
    return this.db.prepare("SELECT * FROM lessons ORDER BY title ASC").all() as Lesson[]; }
  getLesson(id: string): Lesson | undefined { return this.db.prepare("SELECT * FROM lessons WHERE id = ?").get(id) as Lesson | undefined; }
  addQuiz(lessonId: string, question: string, options: string, correctAnswer: string): Quiz {
    const q: Quiz = { id: randomUUID(), lessonId, question, options, correctAnswer, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO quizzes VALUES (?,?,?,?,?,?)").run(q.id, q.lessonId, q.question, q.options, q.correctAnswer, q.createdAt); return q; }
  getQuizzes(lessonId: string): Quiz[] { return this.db.prepare("SELECT * FROM quizzes WHERE lesson_id = ? ORDER BY created_at ASC").all(lessonId) as Quiz[]; }
  enrollStudent(name: string, email: string): Student {
    const s: Student = { id: randomUUID(), name, email, progress: "", enrolledAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO students VALUES (?,?,?,?,?)").run(s.id, s.name, s.email, s.progress, s.enrolledAt); return s; }
  listStudents(): Student[] { return this.db.prepare("SELECT * FROM students ORDER BY name ASC").all() as Student[]; } }
