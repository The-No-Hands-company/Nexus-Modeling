import { createHash, randomBytes } from "node:crypto";
import type { ApiKey } from "./types";

const apiKeys = new Map<string, ApiKey>();
let keyCounter = 0;

function generateApiKey(): { raw: string; hash: string; prefix: string } {
  const raw = `nxk_${randomBytes(32).toString("base64url")}`;
  const hash = createHash("sha256").update(raw).digest("hex");
  const prefix = raw.slice(0, 12);
  return { raw, hash, prefix };
}

function generateKeyId(): string {
  return `key-${Date.now().toString(36)}-${(++keyCounter).toString(36)}`;
}

export function createApiKey(input: {
  userId: string;
  name: string;
  scopes?: string[];
  expiresInDays?: number;
}): { apiKey: ApiKey; rawKey: string } {
  const { raw, hash, prefix } = generateApiKey();
  const now = new Date().toISOString();
  const expiresAt = input.expiresInDays
    ? new Date(Date.now() + input.expiresInDays * 24 * 60 * 60 * 1000).toISOString()
    : undefined;

  const apiKey: ApiKey = {
    id: generateKeyId(),
    userId: input.userId,
    name: input.name,
    keyHash: hash,
    keyPrefix: prefix,
    scopes: input.scopes || [],
    expiresAt,
    revoked: false,
    createdAt: now,
  };

  apiKeys.set(apiKey.id, apiKey);
  return { apiKey, rawKey: raw };
}

export function validateApiKey(rawKey: string): ApiKey | undefined {
  if (!rawKey.startsWith("nxk_")) return undefined;
  const hash = createHash("sha256").update(rawKey).digest("hex");

  for (const key of apiKeys.values()) {
    if (key.keyHash === hash) {
      if (key.revoked) return undefined;
      if (key.expiresAt && new Date(key.expiresAt) < new Date()) return undefined;
      key.lastUsedAt = new Date().toISOString();
      return key;
    }
  }
  return undefined;
}

export function getApiKey(keyId: string): ApiKey | undefined {
  return apiKeys.get(keyId);
}

export function listApiKeys(userId?: string): ApiKey[] {
  const all = Array.from(apiKeys.values());
  return userId ? all.filter((k) => k.userId === userId) : all;
}

export function revokeApiKey(keyId: string): boolean {
  const key = apiKeys.get(keyId);
  if (!key || key.revoked) return false;
  key.revoked = true;
  return true;
}

export function clearApiKeys(): void {
  apiKeys.clear();
  keyCounter = 0;
}
