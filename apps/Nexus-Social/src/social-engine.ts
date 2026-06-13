import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";
export interface Post { id: string; authorId: string; content: string; mediaUrl: string; tags: string; likeCount: number; commentCount: number; createdAt: string; }
export interface Comment { id: string; postId: string; authorId: string; content: string; createdAt: string; }
export interface Like { id: string; postId: string; userId: string; createdAt: string; }
export class SocialEngine { db: Database;
  constructor(p = ":memory:") { this.db = new Database(p);
    this.db.exec("CREATE TABLE IF NOT EXISTS posts (id TEXT PRIMARY KEY, author_id TEXT, content TEXT, media_url TEXT, tags TEXT, like_count INTEGER DEFAULT 0, comment_count INTEGER DEFAULT 0, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS comments (id TEXT PRIMARY KEY, post_id TEXT, author_id TEXT, content TEXT, created_at TEXT)");
    this.db.exec("CREATE TABLE IF NOT EXISTS likes (id TEXT PRIMARY KEY, post_id TEXT, user_id TEXT, created_at TEXT)"); }
  addPost(authorId: string, content: string, mediaUrl = "", tags = ""): Post {
    const p: Post = { id: randomUUID(), authorId, content, mediaUrl, tags, likeCount: 0, commentCount: 0, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO posts VALUES (?,?,?,?,?,?,?,?)").run(p.id, p.authorId, p.content, p.mediaUrl, p.tags, p.likeCount, p.commentCount, p.createdAt); return p; }
  listPosts(tag?: string): Post[] {
    if (tag) return this.db.prepare("SELECT * FROM posts WHERE tags LIKE ? ORDER BY created_at DESC").all(`%${tag}%`) as Post[];
    return this.db.prepare("SELECT * FROM posts ORDER BY created_at DESC").all() as Post[]; }
  getPost(id: string): Post | undefined { return this.db.prepare("SELECT * FROM posts WHERE id = ?").get(id) as Post | undefined; }
  addLike(postId: string, userId: string): Like {
    const l: Like = { id: randomUUID(), postId, userId, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO likes VALUES (?,?,?,?)").run(l.id, l.postId, l.userId, l.createdAt);
    this.db.prepare("UPDATE posts SET like_count = like_count + 1 WHERE id = ?").run(postId); return l; }
  addComment(postId: string, authorId: string, content: string): Comment {
    const c: Comment = { id: randomUUID(), postId, authorId, content, createdAt: new Date().toISOString() };
    this.db.prepare("INSERT INTO comments VALUES (?,?,?,?,?)").run(c.id, c.postId, c.authorId, c.content, c.createdAt);
    this.db.prepare("UPDATE posts SET comment_count = comment_count + 1 WHERE id = ?").run(postId); return c; }
  getComments(postId: string): Comment[] { return this.db.prepare("SELECT * FROM comments WHERE post_id = ? ORDER BY created_at ASC").all(postId) as Comment[]; } }
