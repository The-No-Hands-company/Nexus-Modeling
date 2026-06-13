import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Product { id: string; name: string; sku: string; price: number; category: string; stock: number; createdAt: string; }
export interface Sale { id: string; customerId: string; products: string; total: number; status: string; date: string; createdAt: string; }
export interface Customer { id: string; name: string; email: string; phone: string; createdAt: string; }
export class SalesEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS products (id TEXT PRIMARY KEY, name TEXT, sku TEXT, price REAL, category TEXT, stock INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS sales (id TEXT PRIMARY KEY, customer_id TEXT, products TEXT, total REAL, status TEXT DEFAULT 'pending', date TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS customers (id TEXT PRIMARY KEY, name TEXT, email TEXT, phone TEXT, created_at TEXT)"); }
  addProduct(name: string, sku: string, price: number, category: string, stock = 0): Product {
    const p: Product = { id: randomUUID(), name, sku, price, category, stock, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO products VALUES (?,?,?,?,?,?,?)").run(p.id, p.name, p.sku, p.price, p.category, p.stock, p.createdAt); return p; }
  listProducts(category?: string): Product[] {
    if (category) return this.db.prepare("SELECT * FROM products WHERE category = ? ORDER BY name ASC").all(category) as Product[];
    return this.db.prepare("SELECT * FROM products ORDER BY name ASC").all() as Product[]; }
  getProduct(id: string): Product | undefined { return this.db.prepare("SELECT * FROM products WHERE id = ?").get(id) as Product | undefined; }
  addCustomer(name: string, email: string, phone: string): Customer {
    const c: Customer = { id: randomUUID(), name, email, phone, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO customers VALUES (?,?,?,?,?)").run(c.id, c.name, c.email, c.phone, c.createdAt); return c; }
  listCustomers(): Customer[] { return this.db.prepare("SELECT * FROM customers ORDER BY name ASC").all() as Customer[]; }
  addSale(customerId: string, products: string, total: number, date: string): Sale {
    const s: Sale = { id: randomUUID(), customerId, products, total, status: "completed", date, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO sales VALUES (?,?,?,?,?,?,?)").run(s.id, s.customerId, s.products, s.total, s.status, s.date, s.createdAt); return s; }
  listSales(customerId?: string): Sale[] {
    if (customerId) return this.db.prepare("SELECT * FROM sales WHERE customer_id = ? ORDER BY date DESC").all(customerId) as Sale[];
    return this.db.prepare("SELECT * FROM sales ORDER BY date DESC").all() as Sale[]; } }
