import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface StoreProduct { id: string; name: string; description: string; price: number; category: string; imageUrl: string; stock: number; rating: number; createdAt: string; }
export interface Cart { id: string; userId: string; items: string; total: number; createdAt: string; }
export interface StoreCategory { id: string; name: string; description: string; productCount: number; createdAt: string; }
export class StoreEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS store_products (id TEXT PRIMARY KEY, name TEXT, description TEXT, price REAL, category TEXT, image_url TEXT, stock INTEGER DEFAULT 0, rating REAL DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS carts (id TEXT PRIMARY KEY, user_id TEXT, items TEXT, total REAL DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS store_categories (id TEXT PRIMARY KEY, name TEXT, description TEXT, product_count INTEGER DEFAULT 0, created_at TEXT)"); }
  addProduct(name: string, description: string, price: number, category: string, imageUrl = "", stock = 0, rating = 0): StoreProduct {
    const p: StoreProduct = { id: randomUUID(), name, description, price, category, imageUrl, stock, rating, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO store_products VALUES (?,?,?,?,?,?,?,?,?)").run(p.id, p.name, p.description, p.price, p.category, p.imageUrl, p.stock, p.rating, p.createdAt);
    this.db.prepare("UPDATE store_categories SET product_count = product_count + 1 WHERE id = ?").run(category); return p; }
  listProducts(category?: string): StoreProduct[] {
    if (category) return this.db.prepare("SELECT * FROM store_products WHERE category = ? ORDER BY name ASC").all(category) as StoreProduct[];
    return this.db.prepare("SELECT * FROM store_products ORDER BY name ASC").all() as StoreProduct[]; }
  getProduct(id: string): StoreProduct | undefined { return this.db.prepare("SELECT * FROM store_products WHERE id = ?").get(id) as StoreProduct | undefined; }
  addCategory(name: string, description = ""): StoreCategory {
    const c: StoreCategory = { id: randomUUID(), name, description, productCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO store_categories VALUES (?,?,?,?,?)").run(c.id, c.name, c.description, c.productCount, c.createdAt); return c; }
  listCategories(): StoreCategory[] { return this.db.prepare("SELECT * FROM store_categories ORDER BY name ASC").all() as StoreCategory[]; }
  createCart(userId: string, items: string, total: number): Cart {
    const c: Cart = { id: randomUUID(), userId, items, total, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO carts VALUES (?,?,?,?,?)").run(c.id, c.userId, c.items, c.total, c.createdAt); return c; }
  listCarts(userId?: string): Cart[] {
    if (userId) return this.db.prepare("SELECT * FROM carts WHERE user_id = ? ORDER BY created_at DESC").all(userId) as Cart[];
    return this.db.prepare("SELECT * FROM carts ORDER BY created_at DESC").all() as Cart[]; } }
