import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface MediaAsset { id: string; name: string; type: string; size: number; url: string; tags: string; albumId: string; createdAt: string; }
export interface Album { id: string; name: string; description: string; assetCount: number; createdAt: string; }
export class MediaEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS assets (id TEXT PRIMARY KEY, name TEXT, type TEXT, size INTEGER, url TEXT, tags TEXT DEFAULT '[]', album_id TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS albums (id TEXT PRIMARY KEY, name TEXT, description TEXT, asset_count INTEGER DEFAULT 0, created_at TEXT)");
  }
  createAlbum(name: string, description = ""): Album {
    const a: Album = { id: randomUUID(), name, description, assetCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO albums VALUES (?,?,?,?,?)").run(a.id, a.name, a.description, a.assetCount, a.createdAt); return a;
  }
  listAlbums(): Album[] { return this.db.prepare("SELECT * FROM albums ORDER BY name ASC").all() as Album[]; }
  addAsset(name: string, type: string, size: number, url: string, tags: string[] = [], albumId = ""): MediaAsset {
    const a: MediaAsset = { id: randomUUID(), name, type, size, url, tags: JSON.stringify(tags), albumId, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO assets VALUES (?,?,?,?,?,?,?,?)").run(a.id, a.name, a.type, a.size, a.url, a.tags, a.albumId, a.createdAt);
    if (albumId) this.db.prepare("UPDATE albums SET asset_count = asset_count + 1 WHERE id = ?").run(albumId); return a;
  }
  listAssets(albumId?: string): MediaAsset[] {
    if (albumId) return this.db.prepare("SELECT * FROM assets WHERE album_id = ? ORDER BY created_at DESC").all(albumId) as MediaAsset[];
    return this.db.prepare("SELECT * FROM assets ORDER BY created_at DESC").all() as MediaAsset[];
  }
  getAsset(id: string): MediaAsset | undefined { return this.db.prepare("SELECT * FROM assets WHERE id = ?").get(id) as MediaAsset | undefined; }
}
