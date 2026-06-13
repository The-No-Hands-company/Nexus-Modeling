import type {
  TunnelRoute,
  TunnelExposure,
  TunnelDecision,
  RoutingPolicy,
  RouteHealthStatus,
  TlsCertificate,
  ExposureLifecycle,
  ReachabilityRecord,
} from "./types";
import { mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

let routeCounter = 0;
let decisionCounter = 0;

const routes = new Map<string, TunnelRoute>();
const exposures = new Map<string, TunnelExposure>();
const decisions: TunnelDecision[] = [];
const tlsCerts = new Map<string, TlsCertificate>();
const lifecycles = new Map<string, ExposureLifecycle>();
const reachabilityRecords = new Map<string, ReachabilityRecord>();

const stateFilePath = resolve(
  process.env.NEXUS_TUNNEL_STATE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "tunnel-state.json"),
);

type PersistedTunnelState = {
  version: number;
  counters: {
    routeCounter: number;
    decisionCounter: number;
  };
  routes: TunnelRoute[];
  exposures: TunnelExposure[];
  decisions: TunnelDecision[];
  tlsCerts: TlsCertificate[];
  lifecycles: ExposureLifecycle[];
  reachability: ReachabilityRecord[];
  updatedAt: string;
};

const defaultPolicy: RoutingPolicy = {
  policyId: "default",
  description: "Default tunnel policy — require Guardian approval for public exposure, auto-disable unhealthy routes after 3 consecutive failures",
  maxPublicExposures: 10,
  requireGuardianApproval: true,
  healthCheckIntervalMs: 30000,
  reachabilityCheckIntervalMs: 60000,
  autoDisableUnhealthy: true,
  unhealthyThreshold: 3,
  maxConsecutiveFailures: 3,
};

function persistState(): void {
  const payload: PersistedTunnelState = {
    version: 2,
    counters: { routeCounter, decisionCounter },
    routes: Array.from(routes.values()),
    exposures: Array.from(exposures.values()),
    decisions: [...decisions],
    tlsCerts: Array.from(tlsCerts.values()),
    lifecycles: Array.from(lifecycles.values()),
    reachability: Array.from(reachabilityRecords.values()),
    updatedAt: new Date().toISOString(),
  };

  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    const tempPath = `${stateFilePath}.tmp`;
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch (error) {
    try {
      rmSync(`${stateFilePath}.tmp`, { force: true });
    } catch {
    }
    console.warn(`[nexus-tunnel] Failed to persist state: ${(error as Error).message}`);
  }
}

function hydrateStateFromDisk(): void {
  try {
    const raw = readFileSync(stateFilePath, "utf-8");
    const parsed = JSON.parse(raw) as Partial<PersistedTunnelState>;

    routes.clear();
    exposures.clear();
    decisions.length = 0;
    tlsCerts.clear();
    lifecycles.clear();
    reachabilityRecords.clear();

    for (const route of Array.isArray(parsed.routes) ? parsed.routes : []) {
      if (route && typeof route.toolId === "string") routes.set(route.toolId, route);
    }
    for (const exposure of Array.isArray(parsed.exposures) ? parsed.exposures : []) {
      if (exposure && typeof exposure.toolId === "string") exposures.set(exposure.toolId, exposure);
    }
    for (const decision of Array.isArray(parsed.decisions) ? parsed.decisions : []) {
      decisions.push(decision);
    }
    for (const cert of Array.isArray(parsed.tlsCerts) ? parsed.tlsCerts : []) {
      if (cert && typeof cert.certId === "string") tlsCerts.set(cert.certId, cert);
    }
    for (const lc of Array.isArray(parsed.lifecycles) ? parsed.lifecycles : []) {
      if (lc && typeof lc.toolId === "string") lifecycles.set(lc.toolId, lc);
    }
    for (const rr of Array.isArray(parsed.reachability) ? parsed.reachability : []) {
      if (rr && typeof rr.toolId === "string") reachabilityRecords.set(rr.toolId, rr);
    }

    routeCounter = Math.max(0, Number(parsed.counters?.routeCounter || 0));
    decisionCounter = Math.max(0, Number(parsed.counters?.decisionCounter || 0));
  } catch {
    persistState();
  }
}

function generateId(prefix: string): string {
  return `${prefix}-${Date.now()}-${++routeCounter}`;
}

export function registerRoute(
  toolId: string,
  targetUrl: string,
  exposureMode: "public" | "internal" | "protected" = "internal",
): TunnelRoute {
  const now = new Date().toISOString();
  const existing = routes.get(toolId);

  const route: TunnelRoute = {
    routeId: existing?.routeId ?? generateId("route"),
    toolId,
    targetUrl,
    exposureMode,
    enabled: existing?.enabled ?? true,
    healthStatus: existing?.healthStatus ?? "unknown",
    lastHealthCheck: existing?.lastHealthCheck ?? now,
    consecutiveFailures: existing?.consecutiveFailures ?? 0,
    createdAt: existing?.createdAt ?? now,
    updatedAt: now,
  };

  routes.set(toolId, route);
  persistState();
  return route;
}

export function getRoute(toolId: string): TunnelRoute | undefined {
  return routes.get(toolId);
}

export function setRouteEnabled(toolId: string, enabled: boolean): TunnelRoute | undefined {
  const route = routes.get(toolId);
  if (!route) return undefined;
  route.enabled = enabled;
  route.updatedAt = new Date().toISOString();
  persistState();
  return route;
}

export function updateRouteHealth(
  toolId: string,
  healthStatus: RouteHealthStatus,
  consecutiveFailures: number,
): TunnelRoute | undefined {
  const route = routes.get(toolId);
  if (!route) return undefined;
  route.healthStatus = healthStatus;
  route.consecutiveFailures = consecutiveFailures;
  route.lastHealthCheck = new Date().toISOString();
  route.updatedAt = new Date().toISOString();
  persistState();
  return route;
}

export function listRoutes(): TunnelRoute[] {
  return Array.from(routes.values());
}

export function getEnabledRoutes(): TunnelRoute[] {
  return Array.from(routes.values()).filter((r) => r.enabled);
}

export function recordExposure(
  toolId: string,
  exposureMode: "public" | "internal" | "protected",
  guardianApproved: boolean,
  reason?: string,
  publicUrl?: string,
): TunnelExposure {
  const now = new Date().toISOString();

  const exposure: TunnelExposure = {
    toolId,
    exposureMode,
    publicUrl,
    guardianApproved,
    requestedAt: now,
    approvedAt: guardianApproved ? now : undefined,
    approvalReason: guardianApproved ? reason : undefined,
    denialReason: !guardianApproved ? reason : undefined,
  };

  exposures.set(toolId, exposure);
  persistState();
  return exposure;
}

export function getExposure(toolId: string): TunnelExposure | undefined {
  return exposures.get(toolId);
}

export function listExposures(): TunnelExposure[] {
  return Array.from(exposures.values());
}

export function recordDecision(
  toolId: string,
  exposureMode: "public" | "internal" | "protected",
  approved: boolean,
  decidedBy = "tunnel",
  reason?: string,
): TunnelDecision {
  const decisionId = `dec-${Date.now()}-${++decisionCounter}`;
  const now = new Date().toISOString();

  const decision: TunnelDecision = {
    decisionId,
    toolId,
    exposureMode,
    approved,
    reason,
    decidedAt: now,
    decidedBy,
  };

  decisions.push(decision);
  persistState();
  return decision;
}

export function listDecisions(): TunnelDecision[] {
  return [...decisions];
}

export function upsertTlsCert(input: {
  certId?: string;
  toolId: string;
  domain: string;
  status: TlsCertificate["status"];
  issuer: string;
  expiresAt: string;
  autoRenew: boolean;
}): TlsCertificate {
  const now = new Date().toISOString();
  const existing = input.certId ? tlsCerts.get(input.certId) : undefined;

  const cert: TlsCertificate = {
    certId: input.certId || generateId("cert"),
    toolId: input.toolId,
    domain: input.domain,
    status: input.status,
    issuer: input.issuer,
    expiresAt: input.expiresAt,
    autoRenew: input.autoRenew,
    createdAt: existing?.createdAt ?? now,
    updatedAt: now,
  };

  tlsCerts.set(cert.certId, cert);
  persistState();
  return cert;
}

export function getTlsCert(certId: string): TlsCertificate | undefined {
  return tlsCerts.get(certId);
}

export function listTlsCerts(): TlsCertificate[] {
  return Array.from(tlsCerts.values());
}

export function getTlsCertsForTool(toolId: string): TlsCertificate[] {
  return Array.from(tlsCerts.values()).filter((c) => c.toolId === toolId);
}

export function upsertExposureLifecycle(
  toolId: string,
  exposureMode: TunnelExposure["exposureMode"],
  maxRenewals = 12,
): ExposureLifecycle {
  const now = new Date().toISOString();
  const existing = lifecycles.get(toolId);

  const lifecycle: ExposureLifecycle = {
    toolId,
    exposureMode,
    status: "active",
    issuedAt: existing?.issuedAt ?? now,
    expiresAt: existing?.expiresAt ?? new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString(),
    lastRenewedAt: now,
    renewalCount: (existing?.renewalCount ?? 0) + 1,
    maxRenewals,
  };

  lifecycles.set(toolId, lifecycle);
  persistState();
  return lifecycle;
}

export function getExposureLifecycle(toolId: string): ExposureLifecycle | undefined {
  return lifecycles.get(toolId);
}

export function listExposureLifecycles(): ExposureLifecycle[] {
  return Array.from(lifecycles.values());
}

export function upsertReachabilityRecord(record: ReachabilityRecord): void {
  reachabilityRecords.set(record.toolId, record);
  persistState();
}

export function getReachabilityRecord(toolId: string): ReachabilityRecord | undefined {
  return reachabilityRecords.get(toolId);
}

export function listReachabilityRecords(): ReachabilityRecord[] {
  return Array.from(reachabilityRecords.values());
}

export function getPolicy(): RoutingPolicy {
  return { ...defaultPolicy };
}

export function clearRoutes(): void {
  routes.clear();
  exposures.clear();
  decisions.length = 0;
  tlsCerts.clear();
  lifecycles.clear();
  reachabilityRecords.clear();
  routeCounter = 0;
  decisionCounter = 0;
  persistState();
}

hydrateStateFromDisk();
