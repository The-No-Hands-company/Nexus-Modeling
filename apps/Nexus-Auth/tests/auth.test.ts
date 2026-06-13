import { describe, expect, test } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";
import { seedDefaultIdentities } from "../src/state";
import { issueServiceToken, validateServiceToken } from "../src/token";
import { createUser, authenticateUser, listUsers, clearUsers } from "../src/users";

describe("nexus-auth MVP contracts", () => {
  test("seed flow is idempotent", () => {
    clearUsers();

    const first = seedDefaultIdentities({ adminUsername: "root", userUsername: "demo" });
    expect(first.createdCount).toBe(2);
    expect(first.identities.map((item) => item.username)).toContain("root");
    expect(first.identities.map((item) => item.username)).toContain("demo");

    const second = seedDefaultIdentities({ adminUsername: "root", userUsername: "demo" });
    expect(second.createdCount).toBe(2);
    expect(second.identities.map((item) => item.username)).toContain("root");
    expect(second.identities.map((item) => item.username)).toContain("demo");
  });

  test("token issue and validate roundtrip", () => {
    const issued = issueServiceToken({
      serviceId: "nexus-cloud",
      audience: "nexus-internal",
      scopes: ["service:write"],
      expiresInSeconds: 120,
    });

    const result = validateServiceToken(issued.token, "nexus-internal");
    expect(result.valid).toBe(true);

    if (result.valid) {
      expect(result.payload.sub).toBe("nexus-cloud");
      expect(result.payload.scopes).toEqual(["service:write"]);
    }
  });

  test("systems-api registration payload shape is stable", () => {
    const payload = buildSystemsApiRegistrationPayload("http://localhost:4310");
    expect(payload.id).toBe("nexus-auth");
    expect(payload.name).toBe("Nexus Auth");
    expect(payload.mode).toBe("orchestrated");
    expect(payload.capabilities).toContain("service-auth");
    expect(payload.metadata.supportsServiceTokens).toBe(true);
    expect(payload.metadata.supportsUserManagement).toBe(true);
    expect(payload.metadata.supportsApiKeys).toBe(true);
    expect(payload.metadata.supportsSessions).toBe(true);
  });

  test("user creation and authentication", () => {
    clearUsers();

    const user = createUser({
      username: "testuser",
      email: "test@nexus.local",
      password: "test-password-123",
    });

    expect(user.username).toBe("testuser");
    expect(user.role).toBe("user");
    expect((user as any).passwordHash).toBeUndefined();

    const authenticated = authenticateUser("testuser", "test-password-123");
    expect(authenticated).toBeDefined();
    expect(authenticated!.username).toBe("testuser");

    const failed = authenticateUser("testuser", "wrong-password");
    expect(failed).toBeNull();
  });

  test("user listing", () => {
    clearUsers();

    createUser({ username: "alice", email: "alice@nexus.local", password: "pass1" });
    createUser({ username: "bob", email: "bob@nexus.local", password: "pass2" });

    const users = listUsers();
    expect(users.length).toBe(2);
    expect(users.map((u) => u.username).sort()).toEqual(["alice", "bob"]);
  });
});
