import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Subscription { id: string; customerId: string; plan: string; amount: number; currency: string; interval: "monthly"|"yearly"; status: "active"|"cancelled"|"past_due"; nextBillingDate: string; createdAt: string; }
export interface Payment { id: string; subscriptionId: string; amount: number; currency: string; method: string; status: "completed"|"failed"|"pending"; paidAt: string; }

export class BillingEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(`CREATE TABLE IF NOT EXISTS subscriptions (id TEXT PRIMARY KEY, customer_id TEXT, plan TEXT, amount REAL, currency TEXT DEFAULT 'USD', "interval" TEXT DEFAULT 'monthly', status TEXT DEFAULT 'active', next_billing_date TEXT, created_at TEXT); CREATE TABLE IF NOT EXISTS payments (id TEXT PRIMARY KEY, subscription_id TEXT, amount REAL, currency TEXT, method TEXT, status TEXT DEFAULT 'completed', paid_at TEXT)`);
  }

  createSubscription(customerId: string, plan: string, amount: number, interval: Subscription["interval"] = "monthly"): Subscription {
    const s: Subscription = { id: randomUUID(), customerId, plan, amount, currency: "USD", interval, status: "active", nextBillingDate: new Date(Date.now() + 30*86400000).toISOString().slice(0,10), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO subscriptions VALUES (?,?,?,?,?,?,?,?,?)").run(s.id, s.customerId, s.plan, s.amount, s.currency, s.interval, s.status, s.nextBillingDate, s.createdAt);
    return s;
  }

  listSubscriptions(customerId?: string): Subscription[] {
    if (customerId) return this.db.prepare("SELECT * FROM subscriptions WHERE customer_id = ? ORDER BY created_at DESC").all(customerId) as Subscription[];
    return this.db.prepare("SELECT * FROM subscriptions ORDER BY created_at DESC").all() as Subscription[];
  }

  cancelSubscription(id: string): boolean { return this.db.prepare("UPDATE subscriptions SET status = 'cancelled' WHERE id = ?").run(id).changes > 0; }

  recordPayment(subscriptionId: string, amount: number, method = "card"): Payment {
    const p: Payment = { id: randomUUID(), subscriptionId, amount, currency: "USD", method, status: "completed", paidAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO payments VALUES (?,?,?,?,?,?,?)").run(p.id, p.subscriptionId, p.amount, p.currency, p.method, p.status, p.paidAt);
    return p;
  }

  getRevenue(): { mrr: number; totalPayments: number; activeSubscriptions: number } {
    const active = (this.db.prepare("SELECT COUNT(*) as c FROM subscriptions WHERE status = 'active'").get() as {c:number}).c;
    const mrr = (this.db.prepare("SELECT COALESCE(SUM(amount),0) as t FROM subscriptions WHERE status = 'active' AND \"interval\" = 'monthly'").get() as {t:number}).t + (this.db.prepare("SELECT COALESCE(SUM(amount/12),0) as t FROM subscriptions WHERE status = 'active' AND \"interval\" = 'yearly'").get() as {t:number}).t;
    const payments = (this.db.prepare("SELECT COUNT(*) as c FROM payments").get() as {c:number}).c;
    return { mrr: Math.round(mrr), totalPayments: payments, activeSubscriptions: active };
  }

  close(): void { this.db.close(); }
}
