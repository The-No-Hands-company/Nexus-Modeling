import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Reservations {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class ReservationsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS reservationses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createReservations(name: string, type = ""): Reservations {
    const d: Reservations = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO reservationses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listReservationss(): Reservations[] {
    return this.db.prepare("SELECT * FROM reservationses ORDER BY updated_at DESC").all() as Reservations[];
  }

  getReservations(id: string): Reservations | undefined {
    return this.db.prepare("SELECT * FROM reservationses WHERE id = ?").get(id) as Reservations | undefined;
  }
}
