import type {
  PolicyRule,
  EvaluationRequest,
  EvaluationResult,
  RuleCondition,
  GuardianDecisionVerdict,
} from "./types";

const rules = new Map<string, PolicyRule>();
let ruleCounter = 0;

function generateRuleId(): string {
  return `rule-${Date.now()}-${++ruleCounter}`;
}

function evaluateCondition(condition: RuleCondition, context: Record<string, string>): boolean {
  const fieldValue = (context[condition.field] || "").toLowerCase();
  const conditionValue = condition.value.toLowerCase();

  switch (condition.operator) {
    case "equals":
      return fieldValue === conditionValue;
    case "not-equals":
      return fieldValue !== conditionValue;
    case "contains":
      return fieldValue.includes(conditionValue);
    case "not-contains":
      return !fieldValue.includes(conditionValue);
    case "starts-with":
      return fieldValue.startsWith(conditionValue);
    case "ends-with":
      return fieldValue.endsWith(conditionValue);
    case "matches": {
      try {
        return new RegExp(condition.value, "i").test(fieldValue);
      } catch {
        return false;
      }
    }
    default:
      return false;
  }
}

function allConditionsMatch(rule: PolicyRule, context: Record<string, string>): boolean {
  if (rule.conditions.length === 0) return true;
  return rule.conditions.every((condition) => evaluateCondition(condition, context));
}

function ruleActionToVerdict(action: PolicyRule["action"]): GuardianDecisionVerdict | null {
  switch (action) {
    case "approve":
      return "approve";
    case "deny":
      return "deny";
    case "suspend":
      return "suspend";
    case "quarantine":
      return "quarantine";
    default:
      return null;
  }
}

export function upsertRule(input: {
  id?: string;
  name: string;
  description: string;
  priority: number;
  scopes: PolicyRule["scopes"];
  conditions: RuleCondition[];
  action: PolicyRule["action"];
  metadata?: Record<string, string>;
}): PolicyRule {
  const now = new Date().toISOString();
  const existing = input.id ? rules.get(input.id) : undefined;

  const rule: PolicyRule = {
    id: input.id || generateRuleId(),
    name: input.name,
    description: input.description,
    priority: input.priority,
    enabled: existing?.enabled ?? true,
    scopes: input.scopes,
    conditions: input.conditions,
    action: input.action,
    metadata: input.metadata,
    createdAt: existing?.createdAt ?? now,
    updatedAt: now,
  };

  rules.set(rule.id, rule);
  return rule;
}

export function getRule(id: string): PolicyRule | undefined {
  return rules.get(id);
}

export function listRules(): PolicyRule[] {
  return Array.from(rules.values()).sort((a, b) => b.priority - a.priority);
}

export function deleteRule(id: string): boolean {
  return rules.delete(id);
}

export function setRuleEnabled(id: string, enabled: boolean): PolicyRule | undefined {
  const rule = rules.get(id);
  if (!rule) return undefined;
  rule.enabled = enabled;
  rule.updatedAt = new Date().toISOString();
  return rule;
}

export function evaluateRequest(request: EvaluationRequest, defaultVerdict: GuardianDecisionVerdict): EvaluationResult {
  const evaluatedAt = new Date().toISOString();
  const context = {
    scope: request.scope,
    subjectId: request.subjectId,
    ...request.context,
  };

  const matchingRules = listRules()
    .filter((rule) => rule.enabled)
    .filter((rule) => rule.scopes.length === 0 || rule.scopes.includes(request.scope))
    .filter((rule) => allConditionsMatch(rule, context));

  if (matchingRules.length === 0) {
    return {
      verdict: defaultVerdict,
      reason: `No matching rules — default policy verdict '${defaultVerdict}'`,
      matchedRuleIds: [],
      evaluatedAt,
    };
  }

  const decisive = matchingRules[0];

  if (decisive.action === "log" || decisive.action === "alert") {
    const verdictAction = matchingRules
      .slice(1)
      .map((r) => ruleActionToVerdict(r.action))
      .find((v): v is GuardianDecisionVerdict => v !== null);

    if (verdictAction) {
      return {
        verdict: verdictAction,
        reason: `${decisive.action}: ${decisive.name} — verdict from rule '${matchingRules.find((r) => ruleActionToVerdict(r.action) === verdictAction)?.name}'`,
        matchedRuleIds: matchingRules.map((r) => r.id),
        evaluatedAt,
      };
    }

    return {
      verdict: defaultVerdict,
      reason: `${decisive.action}: ${decisive.name} — default verdict '${defaultVerdict}'`,
      matchedRuleIds: matchingRules.map((r) => r.id),
      evaluatedAt,
    };
  }

  const verdict = ruleActionToVerdict(decisive.action);
  if (!verdict) {
    return {
      verdict: defaultVerdict,
      reason: `Unresolvable action '${decisive.action}' from rule '${decisive.name}' — default verdict '${defaultVerdict}'`,
      matchedRuleIds: matchingRules.map((r) => r.id),
      evaluatedAt,
    };
  }

  return {
    verdict,
    reason: `${verdict}: ${decisive.name} — ${decisive.description}`,
    matchedRuleIds: matchingRules.map((r) => r.id),
    evaluatedAt,
  };
}

export function seedDefaultRules(): PolicyRule[] {
  if (rules.size > 0) return listRules();

  return [
    upsertRule({
      name: "Deny unsafe exposure",
      description: "Deny public exposure for services tagged as unsafe",
      priority: 100,
      scopes: ["exposure"],
      conditions: [
        { field: "safety", operator: "equals", value: "unsafe" },
      ],
      action: "deny",
    }),
    upsertRule({
      name: "Quarantine degraded services on exposure",
      description: "Quarantine exposure requests when service health is degraded",
      priority: 90,
      scopes: ["exposure", "domain"],
      conditions: [
        { field: "health", operator: "equals", value: "degraded" },
      ],
      action: "quarantine",
    }),
    upsertRule({
      name: "Deny offline services",
      description: "Deny any request for services that are offline",
      priority: 80,
      scopes: [],
      conditions: [
        { field: "health", operator: "equals", value: "offline" },
      ],
      action: "deny",
    }),
    upsertRule({
      name: "Block untrusted federation peers",
      description: "Deny runtime operations from untrusted federation peers",
      priority: 70,
      scopes: ["runtime"],
      conditions: [
        { field: "peerTrust", operator: "equals", value: "untrusted" },
      ],
      action: "deny",
    }),
    upsertRule({
      name: "Alert on high-severity threat",
      description: "Log alert when high-severity threat is detected",
      priority: 60,
      scopes: ["agent"],
      conditions: [
        { field: "threatSeverity", operator: "equals", value: "high" },
      ],
      action: "alert",
    }),
    upsertRule({
      name: "Log all exposure requests",
      description: "Audit-log every exposure request for traceability",
      priority: 0,
      scopes: ["exposure", "domain"],
      conditions: [],
      action: "log",
    }),
  ];
}

export function clearRules(): void {
  rules.clear();
  ruleCounter = 0;
}

export function importRules(imported: PolicyRule[]): number {
  let count = 0;
  for (const rule of imported) {
    if (rule.id && rule.name && Array.isArray(rule.conditions)) {
      rules.set(rule.id, rule);
      count++;
    }
  }
  return count;
}
