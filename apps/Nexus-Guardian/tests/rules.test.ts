import { describe, expect, test, beforeEach } from "bun:test";
import {
  upsertRule,
  getRule,
  listRules,
  deleteRule,
  setRuleEnabled,
  evaluateRequest,
  seedDefaultRules,
  clearRules,
} from "../src/rules";

describe("nexus-guardian rule engine", () => {
  beforeEach(() => {
    clearRules();
  });

  test("upsert and retrieve a rule", () => {
    const rule = upsertRule({
      name: "Block unsafe exposure",
      description: "Deny exposure for unsafe tools",
      priority: 100,
      scopes: ["exposure"],
      conditions: [{ field: "safety", operator: "equals", value: "unsafe" }],
      action: "deny",
    });

    expect(rule.id).toBeTruthy();
    expect(rule.name).toBe("Block unsafe exposure");
    expect(rule.priority).toBe(100);
    expect(rule.enabled).toBe(true);

    const retrieved = getRule(rule.id);
    expect(retrieved).toBeDefined();
    expect(retrieved?.action).toBe("deny");
  });

  test("update existing rule by id", () => {
    const original = upsertRule({
      name: "Original",
      description: "Original rule",
      priority: 50,
      scopes: [],
      conditions: [],
      action: "log",
    });

    const updated = upsertRule({
      id: original.id,
      name: "Updated",
      description: "Updated rule",
      priority: 100,
      scopes: ["exposure"],
      conditions: [{ field: "test", operator: "equals", value: "true" }],
      action: "deny",
    });

    expect(updated.id).toBe(original.id);
    expect(updated.name).toBe("Updated");
    expect(updated.priority).toBe(100);
  });

  test("rules are sorted by priority descending", () => {
    upsertRule({ name: "Low", description: "", priority: 10, scopes: [], conditions: [], action: "log" });
    upsertRule({ name: "High", description: "", priority: 100, scopes: [], conditions: [], action: "deny" });
    upsertRule({ name: "Mid", description: "", priority: 50, scopes: [], conditions: [], action: "approve" });

    const rules = listRules();
    expect(rules[0].priority).toBe(100);
    expect(rules[1].priority).toBe(50);
    expect(rules[2].priority).toBe(10);
  });

  test("delete rule", () => {
    const rule = upsertRule({
      name: "To delete",
      description: "",
      priority: 0,
      scopes: [],
      conditions: [],
      action: "log",
    });

    expect(deleteRule(rule.id)).toBe(true);
    expect(getRule(rule.id)).toBeUndefined();
  });

  test("enable and disable rule", () => {
    const rule = upsertRule({
      name: "Toggle me",
      description: "",
      priority: 0,
      scopes: [],
      conditions: [],
      action: "log",
    });

    setRuleEnabled(rule.id, false);
    expect(getRule(rule.id)?.enabled).toBe(false);

    setRuleEnabled(rule.id, true);
    expect(getRule(rule.id)?.enabled).toBe(true);
  });

  test("condition equals operator", () => {
    upsertRule({
      name: "Equals test",
      description: "",
      priority: 100,
      scopes: ["exposure"],
      conditions: [{ field: "toolId", operator: "equals", value: "unsafe-app" }],
      action: "deny",
    });

    const result = evaluateRequest(
      { scope: "exposure", subjectId: "req-1", context: { toolId: "unsafe-app" } },
      "approve",
    );

    expect(result.verdict).toBe("deny");
    expect(result.matchedRuleIds.length).toBe(1);
  });

  test("condition contains operator", () => {
    upsertRule({
      name: "Contains test",
      description: "",
      priority: 100,
      scopes: [],
      conditions: [{ field: "host", operator: "contains", value: "evil" }],
      action: "quarantine",
    });

    const result = evaluateRequest(
      { scope: "domain", subjectId: "evil.example.com", context: { host: "evil.example.com" } },
      "approve",
    );

    expect(result.verdict).toBe("quarantine");
  });

  test("no matching rules returns default verdict", () => {
    upsertRule({
      name: "No match",
      description: "",
      priority: 100,
      scopes: ["exposure"],
      conditions: [{ field: "nonexistent", operator: "equals", value: "never" }],
      action: "deny",
    });

    const result = evaluateRequest(
      { scope: "exposure", subjectId: "req-1", context: {} },
      "approve",
    );

    expect(result.verdict).toBe("approve");
    expect(result.matchedRuleIds.length).toBe(0);
  });

  test("seed default rules", () => {
    clearRules();
    const rules = seedDefaultRules();
    expect(rules.length).toBeGreaterThanOrEqual(5);
    expect(rules.some((r) => r.scopes.includes("exposure"))).toBe(true);
  });

  test("log action falls through to next rule for verdict", () => {
    upsertRule({
      name: "Log only",
      description: "",
      priority: 100,
      scopes: [],
      conditions: [{ field: "subjectId", operator: "equals", value: "test" }],
      action: "log",
    });

    upsertRule({
      name: "Deny it",
      description: "",
      priority: 50,
      scopes: [],
      conditions: [{ field: "subjectId", operator: "equals", value: "test" }],
      action: "deny",
    });

    const result = evaluateRequest(
      { scope: "service", subjectId: "test", context: {} },
      "approve",
    );

    expect(result.verdict).toBe("deny");
    expect(result.matchedRuleIds.length).toBe(2);
  });
});
