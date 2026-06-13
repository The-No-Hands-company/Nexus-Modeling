export type TunnelExposureMode = "public" | "internal" | "protected";

export interface TunnelRoute {
  routeId: string;
  toolId: string;
  targetUrl: string;
  exposureMode: TunnelExposureMode;
  enabled: boolean;
  healthStatus: RouteHealthStatus;
  lastHealthCheck: string;
  consecutiveFailures: number;
  createdAt: string;
  updatedAt: string;
}

export type RouteHealthStatus = "healthy" | "degraded" | "unreachable" | "unknown";

export interface RouteHealthCheck {
  routeId: string;
  toolId: string;
  targetUrl: string;
  status: RouteHealthStatus;
  responseTimeMs: number;
  httpStatus?: number;
  error?: string;
  checkedAt: string;
}

export interface ReachabilityRecord {
  toolId: string;
  targetUrl: string;
  lastReachable: string;
  lastChecked: string;
  consecutiveFailures: number;
  reachable: boolean;
  responseTimeMs: number;
  errorMessage?: string;
}

export interface TunnelExposure {
  toolId: string;
  exposureMode: TunnelExposureMode;
  publicUrl?: string;
  guardianApproved: boolean;
  approvalReason?: string;
  denialReason?: string;
  requestedAt: string;
  approvedAt?: string;
  expiresAt?: string;
  renewedAt?: string;
}

export interface TunnelDecision {
  decisionId: string;
  toolId: string;
  exposureMode: TunnelExposureMode;
  approved: boolean;
  reason?: string;
  decidedAt: string;
  decidedBy: string;
}

export interface RoutingPolicy {
  policyId: string;
  description: string;
  maxPublicExposures: number;
  requireGuardianApproval: boolean;
  healthCheckIntervalMs: number;
  reachabilityCheckIntervalMs: number;
  autoDisableUnhealthy: boolean;
  unhealthyThreshold: number;
  maxConsecutiveFailures: number;
}

export interface TlsCertificate {
  certId: string;
  toolId: string;
  domain: string;
  status: TlsCertStatus;
  issuer: string;
  expiresAt: string;
  autoRenew: boolean;
  createdAt: string;
  updatedAt: string;
}

export type TlsCertStatus = "requested" | "issued" | "renewing" | "expired" | "revoked";

export interface ExposureLifecycle {
  toolId: string;
  exposureMode: TunnelExposureMode;
  status: ExposureLifecycleStatus;
  issuedAt: string;
  expiresAt?: string;
  lastRenewedAt?: string;
  renewalCount: number;
  maxRenewals: number;
}

export type ExposureLifecycleStatus = "active" | "renewing" | "expired" | "revoked";
