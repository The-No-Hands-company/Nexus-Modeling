import type { GuardianDecisionVerdict } from "./types";

export type ThreatSeverity = "low" | "medium" | "high" | "critical";

export interface ThreatPattern {
  id: string;
  name: string;
  category: "injection" | "authentication" | "denial-of-service" | "reconnaissance" | "data-exfiltration" | "malware" | "policy-violation";
  severity: ThreatSeverity;
  patterns: RegExp[];
  description: string;
  autoResponse: "log" | "alert" | "block";
}

export interface ThreatMatch {
  patternId: string;
  patternName: string;
  category: string;
  severity: ThreatSeverity;
  matchedValue: string;
  field: string;
  source: string;
  timestamp: string;
  autoResponse: "log" | "alert" | "block";
}

const patterns = new Map<string, ThreatPattern>();
const matches: ThreatMatch[] = [];
const maxMatches = 1000;

// ── Default Attack Signature Database ──

const DEFAULT_PATTERNS: Omit<ThreatPattern, "id">[] = [
  {
    name: "SQL Injection — Union Select",
    category: "injection",
    severity: "critical",
    patterns: [/union\s+(all\s+)?select\s+/i, /(\%27|\')\s*(or|and)\s+.*=.*(\%27|\')/i, /;\s*drop\s+table/i, /;\s*insert\s+into/i],
    description: "SQL injection attempt using UNION SELECT, tautology, DROP TABLE, or INSERT patterns",
    autoResponse: "block",
  },
  {
    name: "XSS — Script Injection",
    category: "injection",
    severity: "high",
    patterns: [/<script[\s>]/i, /javascript\s*:/i, /onerror\s*=/i, /onload\s*=/i, /<img[^>]+onerror/i, /<svg[^>]+onload/i],
    description: "Cross-site scripting attempt via script tags, event handlers, or javascript: URIs",
    autoResponse: "block",
  },
  {
    name: "Path Traversal",
    category: "injection",
    severity: "high",
    patterns: [/\.\.\/\.\.\//, /\.\.\\\.\.\\/, /%2e%2e%2f/i, /%2e%2e%5c/i, /\/etc\/passwd/i, /\/etc\/shadow/i, /C:\\Windows\\/i],
    description: "Directory traversal attempt targeting system files",
    autoResponse: "block",
  },
  {
    name: "Command Injection",
    category: "injection",
    severity: "critical",
    patterns: [/;\s*(rm|wget|curl|nc|bash|sh|cmd|powershell)\s/i, /\|\s*(rm|wget|curl|nc|bash|sh)\b/i, /\$\{.*\}/, /\`[^`]+\`/],
    description: "Shell command injection via semicolons, pipes, backticks, or $() substitution",
    autoResponse: "block",
  },
  {
    name: "Brute Force — Login Attempts",
    category: "authentication",
    severity: "high",
    patterns: [/password.*(incorrect|invalid|wrong|failed)/i, /login.*failed.*(\d+)/i],
    description: "Repeated failed login attempts indicating brute force attack",
    autoResponse: "alert",
  },
  {
    name: "Credential Stuffing — Common Credentials",
    category: "authentication",
    severity: "high",
    patterns: [/admin.*admin/i, /test.*test/i, /root.*(root|toor|password)/i],
    description: "Common credential pairs indicating credential stuffing attempt",
    autoResponse: "alert",
  },
  {
    name: "Scanner — Vulnerability Probe",
    category: "reconnaissance",
    severity: "medium",
    patterns: [/nikto/i, /sqlmap/i, /nmap/i, /nessus/i, /burpsuite/i, /acunetix/i, /wp-login/i, /\.env$/i, /\.git\//i, /phpmyadmin/i, /wp-admin/i],
    description: "Automated vulnerability scanner or common exploit probe detected via User-Agent or URL",
    autoResponse: "alert",
  },
  {
    name: "DoS — Request Flood Pattern",
    category: "denial-of-service",
    severity: "high",
    patterns: [/X-Forwarded-For:\s*\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3},\s*\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3},\s*\d{1,3}/i, /Range:\s*bytes=0-999999999/i],
    description: "Denial of service indicators — spoofed IP chains or resource exhaustion range requests",
    autoResponse: "block",
  },
  {
    name: "Data Exfiltration — Large Payload",
    category: "data-exfiltration",
    severity: "high",
    patterns: [/Content-Length:\s*[5-9]\d{6,}/i, /boundary=.*Content-Disposition.*filename.*\.(sql|db|dump|tar|gz|zip)/i],
    description: "Unusually large payload or database dump filename pattern",
    autoResponse: "block",
  },
  {
    name: "Malware — Known User-Agent",
    category: "malware",
    severity: "critical",
    patterns: [/Mirai/i, /Gafgyt/i, /Mozi/i, /Hakai/i, /Okiru/i, /Satori/i, /Tsunami/i, /Lizkebab/i, /Tor/i, /Hajime/i],
    description: "Known IoT malware or botnet User-Agent signature",
    autoResponse: "block",
  },
  {
    name: "Policy — Dangerous Method",
    category: "policy-violation",
    severity: "medium",
    patterns: [/^(TRACE|TRACK|DEBUG|OPTIONS)\s/i],
    description: "Use of TRACE, TRACK, DEBUG, or verbose OPTIONS methods indicating reconnaissance or misconfiguration probe",
    autoResponse: "log",
  },
  {
    name: "XXE — External Entity",
    category: "injection",
    severity: "critical",
    patterns: [/<!ENTITY\s+\w+\s+(SYSTEM|PUBLIC)/i, /<!DOCTYPE\s+\w+\s+\[/i],
    description: "XML External Entity injection attempt via DOCTYPE or ENTITY declarations",
    autoResponse: "block",
  },
  {
    name: "SSRF — Internal Network Probe",
    category: "reconnaissance",
    severity: "high",
    patterns: [/http:\/\/(127\.0\.0\.1|localhost|10\.\d{1,3}|172\.(1[6-9]|2\d|3[01])|192\.168|169\.254)/i, /http:\/\/metadata\.google\.internal/i, /http:\/\/169\.254\.169\.254/i],
    description: "Server-Side Request Forgery probing internal networks or cloud metadata endpoints",
    autoResponse: "block",
  },
  {
    name: "Deserialization Attack",
    category: "injection",
    severity: "critical",
    patterns: [/O:\d+:".*"/i, /a:\d+:\{.*\}/i, /s:\d+:".*"/i, /__proto__/i, /constructor\s*\[\s*["']/i],
    description: "PHP/JS object deserialization or prototype pollution attempt",
    autoResponse: "block",
  },
];

// ── Engine ──

export function initPatterns(): void {
  patterns.clear();
  for (const p of DEFAULT_PATTERNS) {
    upsertThreatPattern(p);
  }
}

export function upsertThreatPattern(input: Omit<ThreatPattern, "id"> & { id?: string }): ThreatPattern {
  const id = input.id || `tp-${Date.now()}-${Math.random().toString(36).slice(2)}`;
  const tp: ThreatPattern = { ...input, id };
  patterns.set(id, tp);
  return tp;
}

export function getThreatPattern(id: string): ThreatPattern | undefined {
  return patterns.get(id);
}

export function listThreatPatterns(): ThreatPattern[] {
  return Array.from(patterns.values());
}

export function deleteThreatPattern(id: string): boolean {
  return patterns.delete(id);
}

export function scanInput(input: {
  field: string;
  value: string;
  source: string;
}): ThreatMatch[] {
  const results: ThreatMatch[] = [];
  const now = new Date().toISOString();

  for (const pattern of patterns.values()) {
    for (const regex of pattern.patterns) {
      if (regex.test(input.value)) {
        const match: ThreatMatch = {
          patternId: pattern.id,
          patternName: pattern.name,
          category: pattern.category,
          severity: pattern.severity,
          matchedValue: input.value.slice(0, 200),
          field: input.field,
          source: input.source,
          timestamp: now,
          autoResponse: pattern.autoResponse,
        };
        results.push(match);
        matches.push(match);
        if (matches.length > maxMatches) matches.shift();
        break; // One match per pattern
      }
    }
  }

  return results;
}

export function scanMultiple(inputs: Array<{ field: string; value: string; source: string }>): ThreatMatch[] {
  return inputs.flatMap((input) => scanInput(input));
}

export function getRecentMatches(limit = 50): ThreatMatch[] {
  return matches.slice(-limit).reverse();
}

export function getMatchesForSource(source: string, limit = 50): ThreatMatch[] {
  return matches.filter((m) => m.source === source).slice(-limit).reverse();
}

export function getMatchStats(): {
  totalMatches: number;
  byCategory: Record<string, number>;
  bySeverity: Record<string, number>;
  bySource: Record<string, number>;
} {
  const byCategory: Record<string, number> = {};
  const bySeverity: Record<string, number> = {};
  const bySource: Record<string, number> = {};

  for (const m of matches) {
    byCategory[m.category] = (byCategory[m.category] || 0) + 1;
    bySeverity[m.severity] = (bySeverity[m.severity] || 0) + 1;
    bySource[m.source] = (bySource[m.source] || 0) + 1;
  }

  return { totalMatches: matches.length, byCategory, bySeverity, bySource };
}

export function clearMatches(): void {
  matches.length = 0;
}

initPatterns();
