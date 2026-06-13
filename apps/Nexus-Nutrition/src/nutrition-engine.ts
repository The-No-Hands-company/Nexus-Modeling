import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Food { id: string; name: string; category: string; calories: number; protein: number; carbs: number; fat: number; servingSize: string; createdAt: string; }
export interface Meal { id: string; name: string; foods: string; totalCalories: number; date: string; createdAt: string; }
export class NutritionEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS foods (id TEXT PRIMARY KEY, name TEXT, category TEXT, calories REAL, protein REAL, carbs REAL, fat REAL, serving_size TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS meals (id TEXT PRIMARY KEY, name TEXT, foods TEXT, total_calories REAL, date TEXT, created_at TEXT)"); }
  addFood(name: string, category: string, calories: number, protein: number, carbs: number, fat: number, servingSize: string): Food {
    const f: Food = { id: randomUUID(), name, category, calories, protein, carbs, fat, servingSize, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO foods VALUES (?,?,?,?,?,?,?,?,?)").run(f.id, f.name, f.category, f.calories, f.protein, f.carbs, f.fat, f.servingSize, f.createdAt); return f; }
  listFoods(category?: string): Food[] {
    if (category) return this.db.prepare("SELECT * FROM foods WHERE category = ? ORDER BY name ASC").all(category) as Food[];
    return this.db.prepare("SELECT * FROM foods ORDER BY name ASC").all() as Food[]; }
  getFood(id: string): Food | undefined { return this.db.prepare("SELECT * FROM foods WHERE id = ?").get(id) as Food | undefined; }
  addMeal(name: string, foods: string, totalCalories: number, date: string): Meal {
    const m: Meal = { id: randomUUID(), name, foods, totalCalories, date, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO meals VALUES (?,?,?,?,?,?)").run(m.id, m.name, m.foods, m.totalCalories, m.date, m.createdAt); return m; }
  listMeals(date?: string): Meal[] {
    if (date) return this.db.prepare("SELECT * FROM meals WHERE date = ? ORDER BY created_at DESC").all(date) as Meal[];
    return this.db.prepare("SELECT * FROM meals ORDER BY date DESC, created_at DESC").all() as Meal[]; } }
