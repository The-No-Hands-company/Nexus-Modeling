export type GuardianDecisionVerdict = "approve" | "deny" | "suspend" | "quarantine";

export type GuardianScope = "service" | "user" | "agent" | "network" | "resource" | "exposure" | "domain" | "runtime";

export interface GuardianDecision {
  id: string;
  scope: GuardianScope;
  subjectId: string;
  verdict: GuardianDecisionVerdict;
  reason?: string;
  issuedAt: string;
  issuedBy: string;
  actorRole?: "operator" | "service" | "peer" | "system";
}

export interface GuardianAuditEntry {
  eventId: string;
  decisionId: string;
  scope: GuardianScope;
  subjectId: string;
  verdict: GuardianDecisionVerdict;
  timestamp: string;
  actorRole?: "operator" | "service" | "peer" | "system";
}

export interface GuardianPolicy {
  policyId: string;
  description: string;
  version: number;
  defaultVerdict: GuardianDecisionVerdict;
  enabled: boolean;
  publicExposureDenyRules: string[];
  updatedAt: string;
}

export interface GuardianServiceCapabilities {
  policyEnforcement: boolean;
  threatDetection: boolean;
  auditTrail: boolean;
  supportedScopes: GuardianScope[];
  supportedVerdicts: GuardianDecisionVerdict[];
}

export type RuleConditionOperator =
  | "equals"
  | "not-equals"
  | "contains"
  | "not-contains"
  | "starts-with"
  | "ends-with"
  | "matches";

export interface RuleCondition {
  field: string;
  operator: RuleConditionOperator;
  value: string;
}

export type RuleAction = "approve" | "deny" | "suspend" | "quarantine" | "alert" | "log";

export interface PolicyRule {
  id: string;
  name: string;
  description: string;
  priority: number;
  enabled: boolean;
  scopes: GuardianScope[];
  conditions: RuleCondition[];
  action: RuleAction;
  metadata?: Record<string, string>;
  createdAt: string;
  updatedAt: string;
}

export interface EvaluationRequest {
  scope: GuardianScope;
  subjectId: string;
  context: Record<string, string>;
}

export interface EvaluationResult {
  verdict: GuardianDecisionVerdict;
  reason: string;
  matchedRuleIds: string[];
  evaluatedAt: string;
}

export interface RateLimitConfig {
  id: string;
  scope: GuardianScope;
  maxRequests: number;
  windowMs: number;
  description: string;
  enabled: boolean;
}

export interface RateLimitBucket {
  toolId: string;
  scope: GuardianScope;
  windowStart: number;
  count: number;
}

export interface RateLimitStatus {
  toolId: string;
  limitId: string;
  scope: GuardianScope;
  currentCount: number;
  maxRequests: number;
  windowMs: number;
  remaining: number;
  resetsAt: string;
  throttled: boolean;
}

export type HealthStatus = "healthy" | "degraded" | "offline" | "unknown";

export interface HealthSnapshot {
  toolId: string;
  toolName: string;
  upstreamUrl: string;
  status: HealthStatus;
  lastHeartbeat: string;
  lastChecked: string;
  responseTimeMs: number;
  consecutiveFailures: number;
}

export type AlertLevel = "info" | "warning" | "error" | "critical";

export type AlertChannel = "webhook" | "audit" | "log";

export interface AlertConfig {
  id: string;
  name: string;
  channel: AlertChannel;
  webhookUrl?: string;
  levels: AlertLevel[];
  enabled: boolean;
  eventTypes: string[];
}

export interface Alert {
  id: string;
  configId: string;
  level: AlertLevel;
  eventType: string;
  message: string;
  detail?: Record<string, unknown>;
  createdAt: string;
  dispatched: boolean;
  dispatchError?: string;
}

export interface ThreatResponseRequest {
  toolId: string;
  threatType: string;
  severity: "low" | "medium" | "high" | "critical";
  description: string;
  context?: Record<string, string>;
}

export interface ThreatResponseResult {
  recommendedAction: "log" | "isolate" | "throttle" | "block" | "alert";
  reason: string;
  decisionId?: string;
}
