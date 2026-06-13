import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { clearDecisions } from "../src/state";
import { clearRules, seedDefaultRules } from "../src/rules";
import { clearRateLimits, seedDefaultRateLimits } from "../src/rate-limiter";
import { clearAlertConfigs, clearAlertHistory, seedDefaultAlertConfigs } from "../src/alerts";
import type { ThreatResponseResult } from "../src/types";

let guardianHandle: ReturnType<typeof createGuardianServer> | null = null;

import { createGuardianServer } from "../src/server";

beforeEach(() => {
  clearDecisions();
  clearRules();
  clearRateLimits();
  clearAlertConfigs();
  clearAlertHistory();
  seedDefaultRules();
  seedDefaultRateLimits();
  seedDefaultAlertConfigs();
  process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION = "false";
  process.env.PORT = "0";
  guardianHandle = createGuardianServer();
});

afterEach(() => {
  guardianHandle?.stop();
  guardianHandle = null;
  clearDecisions();
  clearRules();
  clearRateLimits();
  clearAlertConfigs();
  clearAlertHistory();
  delete process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION;
  delete process.env.PORT;
});

describe("nexus-guardian threat response integration", () => {
  test("threat response for low severity tool returns log recommendation", async () => {
    const port = guardianHandle!.server.port;

    const res = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/threat/respond`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        toolId: "nexus-edge",
        threatType: "behavioral",
        severity: "low",
        description: "Slightly unusual request pattern",
      }),
    });

    expect(res.status).toBe(200);
    const payload = await res.json() as { status: string; response: ThreatResponseResult };
    expect(payload.status).toBe("ok");
    expect(payload.response.recommendedAction).toBe("log");
    expect(payload.response.decisionId).toBeTruthy();
  });

  test("threat response for critical severity returns block recommendation", async () => {
    const port = guardianHandle!.server.port;

    const res = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/threat/respond`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        toolId: "nexus-edge",
        threatType: "security",
        severity: "critical",
        description: "Remote code execution attempt",
      }),
    });

    expect(res.status).toBe(200);
    const payload = await res.json() as { status: string; response: ThreatResponseResult };
    expect(payload.response.recommendedAction).toBe("block");
  });

  test("POST /api/v1/guardian/evaluate returns evaluation with decision and rate limit", async () => {
    const port = guardianHandle!.server.port;

    const res = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/evaluate`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        scope: "exposure",
        subjectId: "nexus-host",
        context: { health: "healthy", safety: "safe" },
      }),
    });

    expect(res.status).toBe(200);
    const payload = await res.json() as {
      status: string;
      evaluation: { verdict: string };
      decision: { id: string };
      rateLimit: { throttled: boolean } | null;
    };

    expect(payload.status).toBe("ok");
    expect(payload.evaluation.verdict).toBe("approve");
    expect(payload.decision.id).toBeTruthy();
    expect(payload.rateLimit).toBeDefined();
  });

  test("POST /api/v1/guardian/rules creates a rule and evaluate uses it", async () => {
    const port = guardianHandle!.server.port;

    const ruleRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/rules`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "Block unsafe test",
        description: "Test rule",
        priority: 100,
        scopes: ["exposure"],
        conditions: [{ field: "safety", operator: "equals", value: "unsafe" }],
        action: "deny",
      }),
    });

    expect(ruleRes.status).toBe(201);

    const evalRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/evaluate`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        scope: "exposure",
        subjectId: "bad-tool",
        context: { safety: "unsafe" },
      }),
    });

    expect(evalRes.status).toBe(200);
    const evalPayload = await evalRes.json() as {
      status: string;
      evaluation: { verdict: string; matchedRuleIds: string[] };
    };
    expect(evalPayload.evaluation.verdict).toBe("deny");
    expect(evalPayload.evaluation.matchedRuleIds.length).toBeGreaterThan(0);
  });

  test("POST /api/v1/guardian/rate-limits creates config and GET /api/v1/guardian/rate-limits/:toolId shows status", async () => {
    const port = guardianHandle!.server.port;

    await fetch(`http://127.0.0.1:${port}/api/v1/guardian/rate-limits`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        scope: "exposure",
        maxRequests: 3,
        windowMs: 60000,
        description: "Test",
      }),
    });

    const statusRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/rate-limits/test-tool`);
    expect(statusRes.status).toBe(200);

    const payload = await statusRes.json() as {
      status: string;
      rateLimits: Array<{ scope: string; maxRequests: number }>;
    };
    expect(payload.rateLimits.length).toBeGreaterThan(0);
    expect(payload.rateLimits.some((r) => r.scope === "exposure")).toBe(true);
  });

  test("POST /api/v1/guardian/alerts/config creates alert config", async () => {
    const port = guardianHandle!.server.port;

    const res = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/alerts/config`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "Integration test webhook",
        channel: "log",
        levels: ["warning", "error"],
        eventTypes: ["test_integration"],
      }),
    });

    expect(res.status).toBe(201);
    const payload = await res.json() as { status: string; config: { id: string } };
    expect(payload.config.id).toBeTruthy();
  });
});
