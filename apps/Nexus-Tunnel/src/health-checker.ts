import type { RouteHealthCheck, RouteHealthStatus, TunnelRoute } from "./types";

const healthHistory = new Map<string, RouteHealthCheck[]>();
const healthTimers = new Map<string, ReturnType<typeof setInterval>>();

const maxHistoryPerRoute = 20;

async function probeHealth(targetUrl: string): Promise<{
  status: RouteHealthStatus;
  responseTimeMs: number;
  httpStatus?: number;
  error?: string;
}> {
  const start = Date.now();
  try {
    const response = await fetch(`${targetUrl.replace(/\/$/, "")}/health`, {
      signal: AbortSignal.timeout(5000),
    });
    const responseTimeMs = Date.now() - start;
    if (response.ok) {
      return { status: "healthy", responseTimeMs, httpStatus: response.status };
    }
    return { status: "degraded", responseTimeMs, httpStatus: response.status, error: `HTTP ${response.status}` };
  } catch (err) {
    return { status: "unreachable", responseTimeMs: Date.now() - start, error: (err as Error).message };
  }
}

export async function checkRouteHealth(route: TunnelRoute): Promise<RouteHealthCheck> {
  const result = await probeHealth(route.targetUrl);

  const check: RouteHealthCheck = {
    routeId: route.routeId,
    toolId: route.toolId,
    targetUrl: route.targetUrl,
    status: result.status,
    responseTimeMs: result.responseTimeMs,
    httpStatus: result.httpStatus,
    error: result.error,
    checkedAt: new Date().toISOString(),
  };

  const history = healthHistory.get(route.toolId) || [];
  history.push(check);
  if (history.length > maxHistoryPerRoute) {
    history.shift();
  }
  healthHistory.set(route.toolId, history);

  return check;
}

export function getHealthHistory(toolId: string): RouteHealthCheck[] {
  return healthHistory.get(toolId) || [];
}

export function getLatestHealth(toolId: string): RouteHealthCheck | undefined {
  const history = healthHistory.get(toolId);
  if (!history || history.length === 0) return undefined;
  return history[history.length - 1];
}

export function getAllLatestHealth(): RouteHealthCheck[] {
  const results: RouteHealthCheck[] = [];
  for (const [toolId] of healthHistory) {
    const latest = getLatestHealth(toolId);
    if (latest) results.push(latest);
  }
  return results;
}

export function startRouteHealthChecks(
  routes: TunnelRoute[],
  intervalMs: number,
  onUnhealthy: (route: TunnelRoute, check: RouteHealthCheck) => void,
  unhealthyThreshold: number,
  autoDisableUnhealthy: boolean,
): () => void {
  const timer = setInterval(async () => {
    for (const route of routes) {
      if (!route.enabled) continue;

      const check = await checkRouteHealth(route);

      if (check.status === "unreachable" || check.status === "degraded") {
        const history = healthHistory.get(route.toolId) || [];
        const recentFailures = history.filter(
          (h) => h.status === "unreachable" || h.status === "degraded",
        ).length;

        if (recentFailures >= unhealthyThreshold && autoDisableUnhealthy) {
          onUnhealthy(route, check);
        }
      }
    }
  }, intervalMs);

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}

export function clearHealthData(): void {
  healthHistory.clear();
  for (const [toolId] of healthTimers) {
    const timer = healthTimers.get(toolId);
    if (timer) clearInterval(timer);
  }
  healthTimers.clear();
}
