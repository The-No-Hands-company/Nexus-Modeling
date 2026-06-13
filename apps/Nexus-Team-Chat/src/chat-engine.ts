import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export type ChannelType = "text" | "voice";

export interface Channel {
  id: string;
  name: string;
  type: ChannelType;
  description: string;
  createdAt: string;
  updatedAt: string;
}

export interface Message {
  id: string;
  channelId: string;
  authorId: string;
  authorName: string;
  content: string;
  messageType: "default" | "system" | "reply";
  edited: boolean;
  createdAt: string;
  updatedAt: string;
}

export interface Member {
  userId: string;
  username: string;
  channelId: string;
  joinedAt: string;
}

export type GatewayOp = 
  | "Hello" | "HeartbeatAck" | "Ready" | "Dispatch" | "Reconnect"
  | "Identify" | "Heartbeat" | "CreateMessage" | "TypingStart" 
  | "JoinChannel" | "LeaveChannel" | "CreateChannel" | "PresenceUpdate";

export interface GatewayPayload {
  op: GatewayOp;
  d?: unknown;
  t?: string;
  s?: number;
}

export interface ReadyData {
  user: { id: string; username: string };
  channels: Channel[];
  messages: Record<string, Message[]>;
  members: Record<string, Member[]>;
}

export interface DispatchEvent {
  event: string;
  data: unknown;
  channelId?: string;
}

export class ChatEngine {
  db: Database;
  private eventTarget = new EventTarget();

  constructor(path = ":memory:") {
    this.db = new Database(path);
    this.db.exec("PRAGMA journal_mode=WAL");
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS channels (
        id TEXT PRIMARY KEY, name TEXT NOT NULL, type TEXT DEFAULT 'text',
        description TEXT DEFAULT '', createdAt TEXT NOT NULL, updatedAt TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS messages (
        id TEXT PRIMARY KEY, channel_id TEXT NOT NULL, author_id TEXT NOT NULL,
        author_name TEXT NOT NULL, content TEXT NOT NULL,
        message_type TEXT DEFAULT 'default', edited INTEGER DEFAULT 0,
        created_at TEXT NOT NULL, updated_at TEXT NOT NULL,
        FOREIGN KEY (channel_id) REFERENCES channels(id)
      );
      CREATE TABLE IF NOT EXISTS members (
        user_id TEXT NOT NULL, username TEXT NOT NULL, channel_id TEXT NOT NULL,
        joined_at TEXT NOT NULL,
        PRIMARY KEY (user_id, channel_id),
        FOREIGN KEY (channel_id) REFERENCES channels(id)
      );
      CREATE INDEX IF NOT EXISTS idx_messages_channel ON messages(channel_id, created_at);
    `);
  }

  createChannel(input: { name: string; type?: ChannelType; description?: string }): Channel {
    const now = new Date().toISOString();
    const channel: Channel = {
      id: randomUUID(),
      name: input.name,
      type: input.type || "text",
      description: input.description || "",
      createdAt: now,
      updatedAt: now,
    };
    this.db.prepare("INSERT INTO channels (id, name, type, description, createdAt, updatedAt) VALUES (?, ?, ?, ?, ?, ?)").run(channel.id, channel.name, channel.type, channel.description, channel.createdAt, channel.updatedAt);
    this.emit({ event: "CHANNEL_CREATE", data: channel });
    return channel;
  }

  getChannels(): Channel[] {
    return this.db.prepare("SELECT * FROM channels ORDER BY createdAt").all() as Channel[];
  }

  getChannel(id: string): Channel | undefined {
    return this.db.prepare("SELECT * FROM channels WHERE id = ?").get(id) as Channel | undefined;
  }

  joinChannel(userId: string, username: string, channelId: string): Member {
    const now = new Date().toISOString();
    const member: Member = { userId, username, channelId, joinedAt: now };
    this.db.prepare("INSERT OR REPLACE INTO members (user_id, username, channel_id, joined_at) VALUES (?, ?, ?, ?)").run(userId, username, channelId, now);
    return member;
  }

  leaveChannel(userId: string, channelId: string): void {
    this.db.prepare("DELETE FROM members WHERE user_id = ? AND channel_id = ?").run(userId, channelId);
  }

  getMembers(channelId: string): Member[] {
    return this.db.prepare("SELECT * FROM members WHERE channel_id = ?").all(channelId) as Member[];
  }

  getUserChannels(userId: string): Channel[] {
    const rows = this.db.prepare(`
      SELECT c.* FROM channels c JOIN members m ON c.id = m.channel_id WHERE m.user_id = ?
    `).all(userId) as Channel[];
    return rows;
  }

  createMessage(input: {
    channelId: string; authorId: string; authorName: string;
    content: string; messageType?: Message["messageType"];
  }): Message {
    const now = new Date().toISOString();
    const message: Message = {
      id: randomUUID(),
      channelId: input.channelId,
      authorId: input.authorId,
      authorName: input.authorName,
      content: input.content,
      messageType: input.messageType || "default",
      edited: false,
      createdAt: now,
      updatedAt: now,
    };
    this.db.prepare("INSERT INTO messages (id, channel_id, author_id, author_name, content, message_type, edited, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)").run(message.id, message.channelId, message.authorId, message.authorName, message.content, message.messageType, 0, message.createdAt, message.updatedAt);
    this.emit({ event: "MESSAGE_CREATE", data: message, channelId: message.channelId });
    return message;
  }

  getMessages(channelId: string, limit = 50): Message[] {
    return this.db.prepare("SELECT * FROM messages WHERE channel_id = ? ORDER BY created_at DESC LIMIT ?").all(channelId, limit) as Message[];
  }

  getMessage(id: string): Message | undefined {
    return this.db.prepare("SELECT * FROM messages WHERE id = ?").get(id) as Message | undefined;
  }

  editMessage(id: string, content: string): Message | undefined {
    const msg = this.db.prepare("SELECT * FROM messages WHERE id = ?").get(id) as Message | undefined;
    if (!msg) return undefined;
    msg.content = content;
    msg.edited = true;
    msg.updatedAt = new Date().toISOString();
    this.db.prepare("UPDATE messages SET content = ?, edited = 1, updated_at = ? WHERE id = ?").run(content, msg.updatedAt, id);
    this.emit({ event: "MESSAGE_UPDATE", data: msg, channelId: msg.channelId });
    return msg;
  }

  deleteMessage(id: string): boolean {
    const msg = this.db.prepare("SELECT * FROM messages WHERE id = ?").get(id) as Message | undefined;
    if (!msg) return false;
    this.db.prepare("DELETE FROM messages WHERE id = ?").run(id);
    this.emit({ event: "MESSAGE_DELETE", data: { id, channelId: msg.channelId }, channelId: msg.channelId });
    return true;
  }

  emit(event: DispatchEvent): void {
    this.eventTarget.dispatchEvent(new CustomEvent("dispatch", { detail: event }));
  }

  onDispatch(handler: (event: DispatchEvent) => void): () => void {
    const wrapper = (e: Event) => handler((e as CustomEvent<DispatchEvent>).detail);
    this.eventTarget.addEventListener("dispatch", wrapper);
    return () => this.eventTarget.removeEventListener("dispatch", wrapper);
  }

  close(): void { this.db.close(); }
}
