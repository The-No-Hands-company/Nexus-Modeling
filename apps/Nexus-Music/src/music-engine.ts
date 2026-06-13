import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Track { id: string; title: string; artist: string; album: string; duration: number; genre: string; url: string; createdAt: string; }
export interface Playlist { id: string; name: string; description: string; trackCount: number; createdAt: string; }
export interface PlaylistTrack { id: string; playlistId: string; trackId: string; order: number; addedAt: string; }
export class MusicEngine { db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS tracks (id TEXT PRIMARY KEY, title TEXT, artist TEXT, album TEXT, duration REAL, genre TEXT, url TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS playlists (id TEXT PRIMARY KEY, name TEXT, description TEXT, track_count INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS playlist_tracks (id TEXT PRIMARY KEY, playlist_id TEXT, track_id TEXT, track_order INTEGER, added_at TEXT)");
  }
  addTrack(title: string, artist: string, album: string, duration: number, genre: string, url: string): Track {
    const t: Track = { id: randomUUID(), title, artist, album, duration, genre, url, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO tracks VALUES (?,?,?,?,?,?,?,?)").run(t.id, t.title, t.artist, t.album, t.duration, t.genre, t.url, t.createdAt); return t;
  }
  listTracks(genre?: string): Track[] {
    if (genre) return this.db.prepare("SELECT * FROM tracks WHERE genre = ? ORDER BY title ASC").all(genre) as Track[];
    return this.db.prepare("SELECT * FROM tracks ORDER BY title ASC").all() as Track[];
  }
  getTrack(id: string): Track | undefined { return this.db.prepare("SELECT * FROM tracks WHERE id = ?").get(id) as Track | undefined; }
  createPlaylist(name: string, description = ""): Playlist {
    const p: Playlist = { id: randomUUID(), name, description, trackCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO playlists VALUES (?,?,?,?,?)").run(p.id, p.name, p.description, p.trackCount, p.createdAt); return p;
  }
  listPlaylists(): Playlist[] { return this.db.prepare("SELECT * FROM playlists ORDER BY name ASC").all() as Playlist[]; }
  addToPlaylist(playlistId: string, trackId: string, order: number): PlaylistTrack {
    const pt: PlaylistTrack = { id: randomUUID(), playlistId, trackId, order, addedAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO playlist_tracks VALUES (?,?,?,?,?)").run(pt.id, pt.playlistId, pt.trackId, pt.order, pt.addedAt);
    this.db.prepare("UPDATE playlists SET track_count = track_count + 1 WHERE id = ?").run(playlistId); return pt;
  }
  getPlaylistTracks(playlistId: string): Track[] {
    return this.db.prepare("SELECT t.* FROM tracks t JOIN playlist_tracks pt ON t.id = pt.track_id WHERE pt.playlist_id = ? ORDER BY pt.track_order ASC").all(playlistId) as Track[];
  }
}
