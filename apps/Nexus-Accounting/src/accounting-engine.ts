import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Transaction {
  id: string; accountId: string; type: "income" | "expense" | "transfer";
  amount: number; currency: string; category: string; description: string;
  date: string; reconciled: boolean; createdAt: string;
}
export interface Invoice {
  id: string; clientId: string; number: string; items: InvoiceItem[];
  subtotal: number; tax: number; total: number; currency: string;
  status: "draft" | "sent" | "paid" | "overdue"; dueDate: string; createdAt: string;
}
export interface InvoiceItem { description: string; quantity: number; unitPrice: number; total: number; }

export class AccountingEngine {
  db: Database;
  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec("PRAGMA journal_mode=WAL");
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS transactions (id TEXT PRIMARY KEY, account_id TEXT NOT NULL, type TEXT NOT NULL, amount REAL NOT NULL, currency TEXT DEFAULT 'USD', category TEXT, description TEXT, date TEXT NOT NULL, reconciled INTEGER DEFAULT 0, created_at TEXT NOT NULL);
      CREATE TABLE IF NOT EXISTS invoices (id TEXT PRIMARY KEY, client_id TEXT NOT NULL, number TEXT NOT NULL, items TEXT NOT NULL DEFAULT '[]', subtotal REAL DEFAULT 0, tax REAL DEFAULT 0, total REAL DEFAULT 0, currency TEXT DEFAULT 'USD', status TEXT DEFAULT 'draft', due_date TEXT, created_at TEXT NOT NULL);
    `);
  }

  addTransaction(t: Omit<Transaction, "id" | "createdAt">): Transaction {
    const tx: Transaction = { id: randomUUID(), ...t, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO transactions (id, account_id, type, amount, currency, category, description, date, reconciled, created_at) VALUES (?,?,?,?,?,?,?,?,?,?)").run(tx.id, tx.accountId, tx.type, tx.amount, tx.currency, tx.category, tx.description, tx.date, tx.reconciled ? 1 : 0, tx.createdAt);
    return tx;
  }

  getTransactions(accountId: string, limit = 50): Transaction[] {
    return this.db.prepare("SELECT * FROM transactions WHERE account_id = ? ORDER BY date DESC LIMIT ?").all(accountId, limit) as Transaction[];
  }

  getBalance(accountId: string): { income: number; expense: number; net: number; currency: string } {
    const rows = this.db.prepare("SELECT type, SUM(amount) as total FROM transactions WHERE account_id = ? GROUP BY type").all(accountId) as Array<{ type: string; total: number }>;
    const income = rows.find((r: { type: string }) => r.type === "income")?.total || 0;
    const expense = rows.find((r: { type: string }) => r.type === "expense")?.total || 0;
    return { income, expense, net: income - expense, currency: "USD" };
  }

  reconcile(txId: string): boolean {
    return this.db.prepare("UPDATE transactions SET reconciled = 1 WHERE id = ?").run(txId).changes > 0;
  }

  createInvoice(clientId: string, items: InvoiceItem[], dueDate?: string): Invoice {
    const subtotal = items.reduce((s, i) => s + i.quantity * i.unitPrice, 0);
    const tax = Math.round(subtotal * 0.1 * 100) / 100;
    const total = subtotal + tax;
    const inv: Invoice = { id: randomUUID(), clientId, number: `INV-${Date.now().toString(36).toUpperCase()}`, items, subtotal, tax, total, currency: "USD", status: "draft", dueDate: dueDate || new Date(Date.now() + 30 * 86400000).toISOString().slice(0, 10), createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO invoices (id, client_id, number, items, subtotal, tax, total, currency, status, due_date, created_at) VALUES (?,?,?,?,?,?,?,?,?,?,?)").run(inv.id, inv.clientId, inv.number, JSON.stringify(inv.items), inv.subtotal, inv.tax, inv.total, inv.currency, inv.status, inv.dueDate, inv.createdAt);
    return inv;
  }

  getInvoice(id: string): Invoice | undefined {
    const row = this.db.prepare("SELECT * FROM invoices WHERE id = ?").get(id) as Record<string, unknown> | undefined;
    return row ? { ...row, items: JSON.parse(row.items as string) } as Invoice : undefined;
  }

  listInvoices(clientId?: string): Invoice[] {
    const rows = (clientId ? this.db.prepare("SELECT * FROM invoices WHERE client_id = ? ORDER BY created_at DESC").all(clientId) : this.db.prepare("SELECT * FROM invoices ORDER BY created_at DESC").all()) as Array<Record<string, unknown>>;
    return rows.map((r: Record<string, unknown>) => ({ ...r, items: JSON.parse(r.items as string) } as Invoice));
  }

  updateInvoiceStatus(id: string, status: Invoice["status"]): boolean {
    return this.db.prepare("UPDATE invoices SET status = ? WHERE id = ?").run(status, id).changes > 0;
  }

  getSummary(): { totalIncome: number; totalExpense: number; outstandingInvoices: number; invoiceCount: number } {
    const income = (this.db.prepare("SELECT COALESCE(SUM(amount),0) as t FROM transactions WHERE type = 'income'").get() as { t: number }).t;
    const expense = (this.db.prepare("SELECT COALESCE(SUM(amount),0) as t FROM transactions WHERE type = 'expense'").get() as { t: number }).t;
    const inv = (this.db.prepare("SELECT COUNT(*) as c FROM invoices WHERE status IN ('sent','overdue')").get() as { c: number }).c;
    const total = (this.db.prepare("SELECT COUNT(*) as c FROM invoices").get() as { c: number }).c;
    return { totalIncome: income, totalExpense: expense, outstandingInvoices: inv, invoiceCount: total };
  }

  close(): void { this.db.close(); }
}
