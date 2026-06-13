import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface MindMap { id: string; title: string; description: string; nodeCount: number; createdAt: string; updatedAt: string; }
export interface MindNode { id: string; mapId: string; label: string; content: string; parentId: string; x: number; y: number; createdAt: string; }
export class MindEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS maps (id TEXT PRIMARY KEY, title TEXT, description TEXT, node_count INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS nodes (id TEXT PRIMARY KEY, map_id TEXT, label TEXT, content TEXT DEFAULT '', parent_id TEXT, x REAL DEFAULT 0, y REAL DEFAULT 0, created_at TEXT)");
  }
  createMap(title: string, description = ""): MindMap {
    const m: MindMap = { id: randomUUID(), title, description, nodeCount: 0, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO maps VALUES (?,?,?,?,?,?)").run(m.id, m.title, m.description, m.nodeCount, m.createdAt, m.updatedAt); return m;
  }
  listMaps(): MindMap[] { return this.db.prepare("SELECT * FROM maps ORDER BY updated_at DESC").all() as MindMap[]; }
  getMap(id: string): MindMap | undefined { return this.db.prepare("SELECT * FROM maps WHERE id = ?").get(id) as MindMap | undefined; }
  addNode(mapId: string, label: string, content = "", parentId = "", x = 0, y = 0): MindNode {
    const n: MindNode = { id: randomUUID(), mapId, label, content, parentId, x, y, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO nodes VALUES (?,?,?,?,?,?,?,?)").run(n.id, n.mapId, n.label, n.content, n.parentId, n.x, n.y, n.createdAt);
    this.db.prepare("UPDATE maps SET node_count = node_count + 1, updated_at = ? WHERE id = ?").run(n.createdAt, mapId); return n;
  }
  listNodes(mapId: string): MindNode[] {
    return this.db.prepare("SELECT * FROM nodes WHERE map_id = ? ORDER BY created_at ASC").all(mapId) as MindNode[];
  }
}
