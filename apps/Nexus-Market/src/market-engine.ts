import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Listing { id: string; title: string; description: string; price: number; category: string; seller: string; status: "active" | "sold" | "archived"; createdAt: string; }
export interface Order { id: string; listingId: string; buyer: string; amount: number; status: "pending" | "confirmed" | "shipped" | "delivered"; createdAt: string; }
export class MarketEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS listings (id TEXT PRIMARY KEY, title TEXT, description TEXT, price REAL, category TEXT, seller TEXT, status TEXT DEFAULT 'active', created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS orders (id TEXT PRIMARY KEY, listing_id TEXT, buyer TEXT, amount REAL, status TEXT DEFAULT 'pending', created_at TEXT)");
  }
  createListing(title: string, description: string, price: number, category: string, seller: string): Listing {
    const l: Listing = { id: randomUUID(), title, description, price, category, seller, status: "active", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO listings VALUES (?,?,?,?,?,?,?,?)").run(l.id, l.title, l.description, l.price, l.category, l.seller, l.status, l.createdAt); return l;
  }
  listListings(category?: string): Listing[] {
    if (category) return this.db.prepare("SELECT * FROM listings WHERE category = ? ORDER BY created_at DESC").all(category) as Listing[];
    return this.db.prepare("SELECT * FROM listings ORDER BY created_at DESC").all() as Listing[];
  }
  getListing(id: string): Listing | undefined { return this.db.prepare("SELECT * FROM listings WHERE id = ?").get(id) as Listing | undefined; }
  createOrder(listingId: string, buyer: string, amount: number): Order {
    const o: Order = { id: randomUUID(), listingId, buyer, amount, status: "pending", createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO orders VALUES (?,?,?,?,?,?)").run(o.id, o.listingId, o.buyer, o.amount, o.status, o.createdAt);
    this.db.prepare("UPDATE listings SET status = 'sold' WHERE id = ?").run(listingId); return o;
  }
  listOrders(): Order[] { return this.db.prepare("SELECT * FROM orders ORDER BY created_at DESC").all() as Order[]; }
}
