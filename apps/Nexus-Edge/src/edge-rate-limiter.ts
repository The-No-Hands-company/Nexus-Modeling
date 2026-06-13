import type { RateLimitRule, RateLimitBucket } from "./types";

const rules = new Map<string, RateLimitRule>();
const buckets = new Map<string, RateLimitBucket>();

function bucketKey(host: string, clientIp: string): string {
  return `${host}:${clientIp}`;
}

export function upsertRateLimitRule(input: {
  id?: string;
  host: string;
  maxRequestsPerSecond: number;
  description: string;
}): RateLimitRule {
  const existing = input.id ? rules.get(input.id) : undefined;
  const rule: RateLimitRule = {
    id: input.id || `rlr-${Date.now()}`,
    host: input.host,
    maxRequestsPerSecond: Math.max(1, input.maxRequestsPerSecond),
    enabled: existing?.enabled ?? true,
    description: input.description,
  };

  rules.set(rule.id, rule);
  return rule;
}

export function getRateLimitRule(id: string): RateLimitRule | undefined {
  return rules.get(id);
}

export function listRateLimitRules(): RateLimitRule[] {
  return Array.from(rules.values());
}

export function deleteRateLimitRule(id: string): boolean {
  return rules.delete(id);
}

export function setRateLimitRuleEnabled(id: string, enabled: boolean): RateLimitRule | undefined {
  const rule = rules.get(id);
  if (!rule) return undefined;
  rule.enabled = enabled;
  return rule;
}

export function checkAndRecordRateLimit(host: string, clientIp: string): {
  allowed: boolean;
  remaining: number;
  limit: number;
  retryAfterMs: number;
} {
  const now = Date.now();

  for (const rule of rules.values()) {
    if (!rule.enabled) continue;
    if (rule.host !== host) continue;

    const key = bucketKey(host, clientIp);
    let bucket = buckets.get(key);

    if (!bucket) {
      bucket = { host, clientIp, tokens: rule.maxRequestsPerSecond, lastRefill: now };
      buckets.set(key, bucket);
    }

    const elapsed = (now - bucket.lastRefill) / 1000;
    bucket.tokens = Math.min(rule.maxRequestsPerSecond, bucket.tokens + elapsed * rule.maxRequestsPerSecond);
    bucket.lastRefill = now;

    if (bucket.tokens < 1) {
      const retryAfterMs = Math.ceil((1 - bucket.tokens) * (1000 / rule.maxRequestsPerSecond));
      return {
        allowed: false,
        remaining: 0,
        limit: rule.maxRequestsPerSecond,
        retryAfterMs,
      };
    }

    bucket.tokens -= 1;
    return {
      allowed: true,
      remaining: Math.floor(bucket.tokens),
      limit: rule.maxRequestsPerSecond,
      retryAfterMs: 0,
    };
  }

  return { allowed: true, remaining: -1, limit: -1, retryAfterMs: 0 };
}

export function clearRateLimits(): void {
  rules.clear();
  buckets.clear();
}
