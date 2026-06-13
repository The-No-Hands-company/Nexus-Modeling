import { createHash, randomBytes, scryptSync, timingSafeEqual } from "node:crypto";
import type { User, IdentityRole, Permission } from "./types";
import { mkdirSync, readFileSync, writeFileSync, renameSync, rmSync, dirname, resolve, join, fileURLToPath } from "./types";

const users = new Map<string, User>();
let userCounter = 0;

const stateFilePath = resolve(
  process.env.NEXUS_AUTH_USER_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-users.json"),
);

type PersistedUsers = {
  version: number;
  counter: number;
  users: User[];
  updatedAt: string;
};

function hashPassword(password: string): string {
  const salt = randomBytes(16).toString("hex");
  const hash = scryptSync(password, salt, 64).toString("hex");
  return `${salt}:${hash}`;
}

function verifyPassword(password: string, storedHash: string): boolean {
  const [salt, hash] = storedHash.split(":");
  if (!salt || !hash) return false;
  const computed = scryptSync(password, salt, 64).toString("hex");
  try {
    return timingSafeEqual(Buffer.from(computed), Buffer.from(hash));
  } catch {
    return false;
  }
}

function generateUserId(): string {
  return `usr-${Date.now().toString(36)}-${(++userCounter).toString(36)}`;
}

function hashToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

function persistUsers(): void {
  const payload: PersistedUsers = {
    version: 1,
    counter: userCounter,
    users: Array.from(users.values()),
    updatedAt: new Date().toISOString(),
  };

  const tempPath = `${stateFilePath}.tmp`;
  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch {
    try { rmSync(tempPath, { force: true }); } catch {}
  }
}

function hydrateUsers(): void {
  try {
    const raw = readFileSync(stateFilePath, "utf-8");
    const parsed = JSON.parse(raw) as Partial<PersistedUsers>;
    users.clear();
    for (const user of Array.isArray(parsed.users) ? parsed.users : []) {
      if (user && typeof user.id === "string" && typeof user.username === "string") {
        users.set(user.id, user);
      }
    }
    userCounter = Math.max(0, Number(parsed.counter || 0));
  } catch {
    persistUsers();
  }
}

export function createUser(input: {
  username: string;
  email: string;
  password: string;
  role?: IdentityRole;
}): Omit<User, "passwordHash" | "totpSecret"> {
  if (findUserByUsername(input.username)) {
    throw new Error(`Username '${input.username}' already exists`);
  }

  const now = new Date().toISOString();
  const user: User = {
    id: generateUserId(),
    username: input.username.trim().toLowerCase(),
    email: input.email.trim().toLowerCase(),
    role: input.role || "user",
    passwordHash: hashPassword(input.password),
    totpEnabled: false,
    disabled: false,
    createdAt: now,
    updatedAt: now,
  };

  users.set(user.id, user);
  persistUsers();
  return sanitizeUser(user);
}

export function authenticateUser(username: string, password: string): Omit<User, "passwordHash" | "totpSecret"> | null {
  const user = findUserByUsername(username);
  if (!user || user.disabled) return null;

  if (!verifyPassword(password, user.passwordHash)) return null;

  user.lastLoginAt = new Date().toISOString();
  users.set(user.id, user);
  persistUsers();

  return sanitizeUser(user);
}

export function findUserByUsername(username: string): User | undefined {
  const normalized = username.trim().toLowerCase();
  for (const user of users.values()) {
    if (user.username === normalized) return user;
  }
  return undefined;
}

export function findUserByEmail(email: string): User | undefined {
  const normalized = email.trim().toLowerCase();
  for (const user of users.values()) {
    if (user.email === normalized) return user;
  }
  return undefined;
}

export function getUser(userId: string): Omit<User, "passwordHash" | "totpSecret"> | undefined {
  const user = users.get(userId);
  return user ? sanitizeUser(user) : undefined;
}

export function listUsers(): Omit<User, "passwordHash" | "totpSecret">[] {
  return Array.from(users.values()).map(sanitizeUser);
}

export function updateUser(userId: string, patch: {
  email?: string;
  role?: IdentityRole;
  disabled?: boolean;
}): Omit<User, "passwordHash" | "totpSecret"> | undefined {
  const user = users.get(userId);
  if (!user) return undefined;

  if (patch.email !== undefined) user.email = patch.email.trim().toLowerCase();
  if (patch.role !== undefined) user.role = patch.role;
  if (patch.disabled !== undefined) user.disabled = patch.disabled;
  user.updatedAt = new Date().toISOString();

  users.set(user.id, user);
  persistUsers();
  return sanitizeUser(user);
}

export function changePassword(userId: string, currentPassword: string, newPassword: string): boolean {
  const user = users.get(userId);
  if (!user) return false;
  if (!verifyPassword(currentPassword, user.passwordHash)) return false;

  user.passwordHash = hashPassword(newPassword);
  user.updatedAt = new Date().toISOString();
  users.set(user.id, user);
  persistUsers();
  return true;
}

export function deleteUser(userId: string): boolean {
  const deleted = users.delete(userId);
  if (deleted) persistUsers();
  return deleted;
}

export function userHasPermission(userId: string, permission: Permission): boolean {
  const user = users.get(userId);
  if (!user || user.disabled) return false;

  const rolePermissions: Record<IdentityRole, Permission[]> = {
    founder: [
      "auth:admin", "auth:read", "users:create", "users:read", "users:update", "users:delete",
      "tokens:issue", "tokens:revoke", "tokens:validate",
      "apikeys:create", "apikeys:revoke", "apikeys:read",
      "sessions:read", "sessions:revoke", "system:health", "system:config",
    ],
    admin: [
      "auth:read", "users:create", "users:read", "users:update", "users:delete",
      "tokens:issue", "tokens:revoke", "tokens:validate",
      "apikeys:create", "apikeys:revoke", "apikeys:read",
      "sessions:read", "sessions:revoke", "system:health", "system:config",
    ],
    operator: [
      "auth:read", "users:read", "tokens:validate",
      "apikeys:read", "sessions:read", "system:health",
    ],
    user: ["auth:read", "system:health"],
    viewer: ["auth:read"],
  };

  return (rolePermissions[user.role] || []).includes(permission);
}

export function sanitizeUser(user: User): Omit<User, "passwordHash" | "totpSecret"> {
  const { passwordHash, totpSecret, ...safe } = user;
  return safe;
}

export function seedDefaultUsers(adminUsername?: string, operatorUsername?: string): Omit<User, "passwordHash" | "totpSecret">[] {
  const results: Omit<User, "passwordHash" | "totpSecret">[] = [];
  const admin = adminUsername?.trim().toLowerCase() || "founder";
  const operator = operatorUsername?.trim().toLowerCase() || "operator";

  if (!findUserByUsername(admin)) {
    results.push(createUser({
      username: admin,
      email: `${admin}@nexus.local`,
      password: process.env.NEXUS_AUTH_FOUNDER_PASSWORD || "nexus-founder-2026",
      role: "founder",
    }));
  }

  if (!findUserByUsername(operator)) {
    results.push(createUser({
      username: operator,
      email: `${operator}@nexus.local`,
      password: process.env.NEXUS_AUTH_OPERATOR_PASSWORD || "nexus-operator-2026",
      role: "operator",
    }));
  }

  return results;
}

export function clearUsers(): void {
  users.clear();
  userCounter = 0;
  persistUsers();
}

hydrateUsers();
