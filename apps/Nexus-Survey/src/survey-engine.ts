import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Survey { id: string; title: string; description: string; questions: string; active: number; createdAt: string; }
export interface SurveyResponse { id: string; surveyId: string; respondentId: string; answers: string; score: number; submittedAt: string; }
export class SurveyEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS surveys (id TEXT PRIMARY KEY, title TEXT, description TEXT, questions TEXT, active INTEGER DEFAULT 1, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS survey_responses (id TEXT PRIMARY KEY, survey_id TEXT, respondent_id TEXT, answers TEXT, score REAL, submitted_at TEXT)"); }
  addSurvey(title: string, description: string, questions: string): Survey {
    const s: Survey = { id: randomUUID(), title, description, questions, active: 1, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO surveys VALUES (?,?,?,?,?,?)").run(s.id, s.title, s.description, s.questions, s.active, s.createdAt); return s; }
  listSurveys(active?: boolean): Survey[] {
    if (active !== undefined) return this.db.prepare("SELECT * FROM surveys WHERE active = ? ORDER BY created_at DESC").all(active ? 1 : 0) as Survey[];
    return this.db.prepare("SELECT * FROM surveys ORDER BY created_at DESC").all() as Survey[]; }
  getSurvey(id: string): Survey | undefined { return this.db.prepare("SELECT * FROM surveys WHERE id = ?").get(id) as Survey | undefined; }
  submitResponse(surveyId: string, respondentId: string, answers: string, score: number): SurveyResponse {
    const sr: SurveyResponse = { id: randomUUID(), surveyId, respondentId, answers, score, submittedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO survey_responses VALUES (?,?,?,?,?,?)").run(sr.id, sr.surveyId, sr.respondentId, sr.answers, sr.score, sr.submittedAt); return sr; }
  getResponses(surveyId: string): SurveyResponse[] { return this.db.prepare("SELECT * FROM survey_responses WHERE survey_id = ? ORDER BY submitted_at DESC").all(surveyId) as SurveyResponse[]; } }
