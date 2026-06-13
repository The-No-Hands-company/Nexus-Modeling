export interface IpRecord {
  ip: string;
  score: number;
  requestCount: number;
  blockedCount: number;
  lastSeen: string;
  firstSeen: string;
  blacklisted: boolean;
  blacklistedAt?: string;
  blacklistReason?: string;
  country?: string;
  isp?: string;
  tags: string[];
}

export interface IpReputationConfig {
  maxScore: number;
  blockThreshold: number;
  decayRatePerMinute: number;
  maxRequestsPerMinute: number;
  maxBlockedRequestsPerMinute: number;
}

const records = new Map<string, IpRecord>();
const config: IpReputationConfig = {
  maxScore: 100,
  blockThreshold: 70,
  decayRatePerMinute: 2,
  maxRequestsPerMinute: 200,
  maxBlockedRequestsPerMinute: 20,
};

function getOrCreate(ip: string): IpRecord {
  const existing = records.get(ip);
  if (existing) return existing;

  const record: IpRecord = {
    ip,
    score: 0,
    requestCount: 0,
    blockedCount: 0,
    lastSeen: new Date().toISOString(),
    firstSeen: new Date().toISOString(),
    blacklisted: false,
    tags: [],
  };
  records.set(ip, record);
  return record;
}

export function recordRequest(ip: string): IpRecord {
  const record = getOrCreate(ip);
  record.requestCount++;
  record.lastSeen = new Date().toISOString();
  return record;
}

export function recordBlock(ip: string, reason: string): IpRecord {
  const record = getOrCreate(ip);
  record.blockedCount++;
  record.score = Math.min(config.maxScore, record.score + 15);
  record.lastSeen = new Date().toISOString();
  record.tags.push(reason);

  if (record.score >= config.blockThreshold && !record.blacklisted) {
    record.blacklisted = true;
    record.blacklistedAt = new Date().toISOString();
    record.blacklistReason = reason;
  }

  records.set(ip, record);
  return record;
}

export function addScore(ip: string, points: number, reason: string): IpRecord {
  const record = getOrCreate(ip);
  record.score = Math.min(config.maxScore, record.score + points);
  record.lastSeen = new Date().toISOString();
  record.tags.push(reason);

  if (record.score >= config.blockThreshold && !record.blacklisted) {
    record.blacklisted = true;
    record.blacklistedAt = new Date().toISOString();
    record.blacklistReason = reason;
  }

  records.set(ip, record);
  return record;
}

export function isBlacklisted(ip: string): boolean {
  return records.get(ip)?.blacklisted || false;
}

export function getIpRecord(ip: string): IpRecord | undefined {
  return records.get(ip);
}

export function listIpRecords(): IpRecord[] {
  return Array.from(records.values()).sort((a, b) => b.score - a.score);
}

export function getBlacklistedIps(): IpRecord[] {
  return Array.from(records.values()).filter((r) => r.blacklisted);
}

export function unblacklist(ip: string): boolean {
  const record = records.get(ip);
  if (!record) return false;
  record.blacklisted = false;
  record.blacklistedAt = undefined;
  record.blacklistReason = undefined;
  record.score = Math.max(0, record.score - 30);
  records.set(ip, record);
  return true;
}

export function applyDecay(): number {
  let decayedCount = 0;
  for (const record of records.values()) {
    if (record.score > 0) {
      record.score = Math.max(0, record.score - config.decayRatePerMinute);
      decayedCount++;
    }
  }
  return decayedCount;
}

export function getStats(): {
  totalIps: number;
  blacklistedCount: number;
  highRiskCount: number;
  totalRequests: number;
} {
  let blacklistedCount = 0;
  let highRiskCount = 0;
  let totalRequests = 0;

  for (const r of records.values()) {
    totalRequests += r.requestCount;
    if (r.blacklisted) blacklistedCount++;
    if (r.score >= 50 && !r.blacklisted) highRiskCount++;
  }

  return { totalIps: records.size, blacklistedCount, highRiskCount, totalRequests };
}

export function clearReputation(): void {
  records.clear();
}
