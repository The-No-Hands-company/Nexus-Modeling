import type { IdentityRole } from "./types";
import { listUsers, seedDefaultUsers } from "./users";

export type SeedIdentity = {
  id: string;
  username: string;
  role: IdentityRole;
  createdAt: string;
};

export function seedDefaultIdentities(input?: { adminUsername?: string; userUsername?: string }) {
  const admin = input?.adminUsername?.trim();
  const user = input?.userUsername?.trim();
  seedDefaultUsers(admin, user);
  const identities = listUsers();

  return {
    createdCount: identities.length,
    identities: identities.map((u) => ({
      id: u.id,
      username: u.username,
      role: u.role,
      createdAt: u.createdAt,
    })),
  };
}

export function listSeedIdentities(): SeedIdentity[] {
  return listUsers().map((u) => ({
    id: u.id,
    username: u.username,
    role: u.role,
    createdAt: u.createdAt,
  }));
}
