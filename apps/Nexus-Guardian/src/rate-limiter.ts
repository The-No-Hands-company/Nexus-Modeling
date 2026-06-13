import type { RateLimitConfig, RateLimitBucket, RateLimitStatus, GuardianScope } from "./types";

const configs = new Map<string, RateLimitConfig>();
const buckets = new Map<string, RateLimitBucket[]>();
let configCounter = 0;

function generateConfigId(): string {
  return `rl-${Date.now()}-${++configCounter}`;
}

function bucketKey(toolId: string, scope: GuardianScope): string {
  return `${toolId}:${scope}`;
}

function pruneExpiredBuckets(toolId: string, scope: GuardianScope): void {
  const key = bucketKey(toolId, scope);
  const existing = buckets.get(key);
  if (!existing) return;

  const now = Date.now();
  const active = existing.filter((b) => now - b.windowStart < b.windowStart + 3600000);
  buckets.set(key, active.length > 0 ? active : []);
}

export function upsertRateLimitConfig(input: {
  id?: string;
  scope: GuardianScope;
  maxRequests: number;
  windowMs: number;
  description: string;
}): RateLimitConfig {
  const now = new Date().toISOString();
  const existing = input.id ? configs.get(input.id) : undefined;

  const config: RateLimitConfig = {
    id: input.id || generateConfigId(),
    scope: input.scope,
    maxRequests: input.maxRequests,
    windowMs: input.windowMs,
    description: input.description,
    enabled: existing?.enabled ?? true,
  };

  configs.set(config.id, config);
  return config;
}

export function getRateLimitConfig(id: string): RateLimitConfig | undefined {
  return configs.get(id);
}

export function listRateLimitConfigs(): RateLimitConfig[] {
  return Array.from(configs.values());
}

export function deleteRateLimitConfig(id: string): boolean {
  return configs.delete(id);
}

export function setRateLimitConfigEnabled(id: string, enabled: boolean): RateLimitConfig | undefined {
  const config = configs.get(id);
  if (!config) return undefined;
  config.enabled = enabled;
  return config;
}

export function checkRateLimit(toolId: string, scope: GuardianScope): RateLimitStatus | null {
  const now = Date.now();

  for (const config of configs.values()) {
    if (!config.enabled) continue;
    if (config.scope === scope) {
      const key = bucketKey(toolId, scope);
      let toolBuckets = buckets.get(key) || [];

      pruneExpiredBuckets(toolId, scope);
      toolBuckets = buckets.get(key) || [];

      const windowStart = now - config.windowMs;
      const recentBuckets = toolBuckets.filter((b) => b.windowStart >= windowStart);
      const totalCount = recentBuckets.reduce((sum, b) => sum + b.count, 0);

      const remaining = Math.max(0, config.maxRequests - totalCount - 1);
      const throttled = totalCount >= config.maxRequests;

      return {
        toolId,
        limitId: config.id,
        scope,
        currentCount: totalCount + 1,
        maxRequests: config.maxRequests,
        windowMs: config.windowMs,
        remaining,
        resetsAt: new Date(windowStart + config.windowMs).toISOString(),
        throttled,
      };
    }
  }

  return null;
}

export function recordRequest(toolId: string, scope: GuardianScope): void {
  const key = bucketKey(toolId, scope);
  const now = Date.now();
  let toolBuckets = buckets.get(key) || [];

  const currentBucket = toolBuckets.find((b) => b.windowStart === now);
  if (currentBucket) {
    currentBucket.count++;
  } else {
    toolBuckets.push({ toolId, scope, windowStart: now, count: 1 });
  }

  buckets.set(key, toolBuckets);
}

export function getRateLimitStatus(toolId: string): RateLimitStatus[] {
  const results: RateLimitStatus[] = [];
  const now = Date.now();

  for (const config of configs.values()) {
    if (!config.enabled) continue;

    const key = bucketKey(toolId, config.scope);
    const toolBuckets = buckets.get(key) || [];
    const recentCount = toolBuckets
      .filter((b) => b.windowStart >= now - config.windowMs)
      .reduce((sum, b) => sum + b.count, 0);

    const remaining = Math.max(0, config.maxRequests - recentCount);
    results.push({
      toolId,
      limitId: config.id,
      scope: config.scope,
      currentCount: recentCount,
      maxRequests: config.maxRequests,
      windowMs: config.windowMs,
      remaining,
      resetsAt: new Date(now + config.windowMs).toISOString(),
      throttled: recentCount >= config.maxRequests,
    });
  }

  return results;
}

export function seedDefaultRateLimits(): RateLimitConfig[] {
  if (configs.size > 0) return listRateLimitConfigs();

  return [
    upsertRateLimitConfig({
      scope: "exposure",
      maxRequests: 10,
      windowMs: 60000,
      description: "Max 10 exposure requests per minute per tool",
    }),
    upsertRateLimitConfig({
      scope: "domain",
      maxRequests: 5,
      windowMs: 60000,
      description: "Max 5 domain binding requests per minute per tool",
    }),
    upsertRateLimitConfig({
      scope: "runtime",
      maxRequests: 30,
      windowMs: 60000,
      description: "Max 30 runtime operations per minute per tool",
    }),
  ];
}

export function clearRateLimits(): void {
  configs.clear();
  buckets.clear();
  configCounter = 0;
}
