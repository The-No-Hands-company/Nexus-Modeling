import { describe, expect, test, beforeEach } from "bun:test";
import {
  upsertRateLimitConfig,
  listRateLimitConfigs,
  deleteRateLimitConfig,
  setRateLimitConfigEnabled,
  checkRateLimit,
  recordRequest,
  getRateLimitStatus,
  seedDefaultRateLimits,
  clearRateLimits,
} from "../src/rate-limiter";

describe("nexus-guardian rate limiter", () => {
  beforeEach(() => {
    clearRateLimits();
  });

  test("upsert and list rate limit configs", () => {
    upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 5,
      windowMs: 60000,
      description: "Test limit",
    });

    const configs = listRateLimitConfigs();
    expect(configs.length).toBe(1);
    expect(configs[0].scope).toBe("exposure");
    expect(configs[0].maxRequests).toBe(5);
  });

  test("delete rate limit config", () => {
    const config = upsertRateLimitConfig({
      scope: "domain",
      maxRequests: 3,
      windowMs: 30000,
      description: "",
    });

    expect(deleteRateLimitConfig(config.id)).toBe(true);
    expect(listRateLimitConfigs().length).toBe(0);
  });

  test("check rate limit returns status", () => {
    upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 3,
      windowMs: 60000,
      description: "",
    });

    const status = checkRateLimit("tool-1", "exposure");
    expect(status).toBeDefined();
    expect(status!.toolId).toBe("tool-1");
    expect(status!.scope).toBe("exposure");
    expect(status!.throttled).toBe(false);
  });

  test("rate limit throttles after exceeding max requests", () => {
    upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 2,
      windowMs: 60000,
      description: "",
    });

    recordRequest("tool-1", "exposure");
    recordRequest("tool-1", "exposure");

    const status = checkRateLimit("tool-1", "exposure");
    expect(status).toBeDefined();
    expect(status!.throttled).toBe(true);
    expect(status!.remaining).toBe(0);
  });

  test("get rate limit status for specific tool", () => {
    upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 10,
      windowMs: 60000,
      description: "",
    });

    upsertRateLimitConfig({
      scope: "domain",
      maxRequests: 5,
      windowMs: 60000,
      description: "",
    });

    recordRequest("tool-1", "exposure");
    recordRequest("tool-1", "exposure");

    const statuses = getRateLimitStatus("tool-1");
    expect(statuses.length).toBe(2);

    const exposureStatus = statuses.find((s) => s.scope === "exposure");
    expect(exposureStatus).toBeDefined();
    expect(exposureStatus!.currentCount).toBe(2);
  });

  test("disable rate limit config", () => {
    const config = upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 1,
      windowMs: 60000,
      description: "",
    });

    recordRequest("tool-1", "exposure");

    setRateLimitConfigEnabled(config.id, false);

    const status = checkRateLimit("tool-1", "exposure");
    expect(status).toBeNull();
  });

  test("seed default rate limits", () => {
    clearRateLimits();
    const configs = seedDefaultRateLimits();
    expect(configs.length).toBe(3);
    expect(configs.some((c) => c.scope === "exposure")).toBe(true);
    expect(configs.some((c) => c.scope === "domain")).toBe(true);
    expect(configs.some((c) => c.scope === "runtime")).toBe(true);
  });
});
