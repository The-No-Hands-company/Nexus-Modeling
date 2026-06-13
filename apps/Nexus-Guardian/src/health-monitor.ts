import type { HealthSnapshot, HealthStatus } from "./types";

const snapshots = new Map<string, HealthSnapshot>();
const monitorTimers = new Map<string, ReturnType<typeof setInterval>>();

async function probeHealth(url: string): Promise<{ ok: boolean; responseTimeMs: number }> {
  const start = Date.now();
  try {
    const response = await fetch(url, { signal: AbortSignal.timeout(5000) });
    const responseTimeMs = Date.now() - start;
    return { ok: response.ok, responseTimeMs };
  } catch {
    return { ok: false, responseTimeMs: Date.now() - start };
  }
}

export function registerToolForMonitoring(input: {
  toolId: string;
  toolName: string;
  upstreamUrl: string;
}): HealthSnapshot {
  const now = new Date().toISOString();
  const snapshot: HealthSnapshot = {
    toolId: input.toolId,
    toolName: input.toolName,
    upstreamUrl: input.upstreamUrl,
    status: "unknown",
    lastHeartbeat: now,
    lastChecked: now,
    responseTimeMs: 0,
    consecutiveFailures: 0,
  };

  snapshots.set(input.toolId, snapshot);
  return snapshot;
}

export function heartbeatTool(
  toolId: string,
  toolName: string,
  upstreamUrl: string,
  health: HealthStatus,
): HealthSnapshot {
  const now = new Date().toISOString();
  const existing = snapshots.get(toolId);

  const snapshot: HealthSnapshot = {
    toolId,
    toolName: toolName || existing?.toolName || toolId,
    upstreamUrl: upstreamUrl || existing?.upstreamUrl || "",
    status: health,
    lastHeartbeat: now,
    lastChecked: now,
    responseTimeMs: existing?.responseTimeMs || 0,
    consecutiveFailures: health === "offline" ? (existing?.consecutiveFailures || 0) + 1 : 0,
  };

  snapshots.set(toolId, snapshot);
  return snapshot;
}

export function getHealthSnapshot(toolId: string): HealthSnapshot | undefined {
  return snapshots.get(toolId);
}

export function listHealthSnapshots(): HealthSnapshot[] {
  return Array.from(snapshots.values());
}

export function deregisterTool(toolId: string): boolean {
  stopMonitoring(toolId);
  return snapshots.delete(toolId);
}

export function startMonitoring(
  toolId: string,
  toolName: string,
  upstreamUrl: string,
  intervalMs = 30000,
): HealthSnapshot {
  const snapshot = registerToolForMonitoring({ toolId, toolName, upstreamUrl });

  stopMonitoring(toolId);

  const timer = setInterval(async () => {
    const healthUrl = `${upstreamUrl.replace(/\/$/, "")}/health`;
    const { ok, responseTimeMs } = await probeHealth(healthUrl);

    let status: HealthStatus;
    if (ok) {
      status = "healthy";
    } else {
      const current = snapshots.get(toolId);
      const failures = (current?.consecutiveFailures || 0) + 1;
      status = failures >= 3 ? "offline" : "degraded";
    }

    const updated = heartbeatTool(toolId, toolName, upstreamUrl, status);
    updated.responseTimeMs = responseTimeMs;
    updated.lastChecked = new Date().toISOString();
    if (ok) {
      updated.consecutiveFailures = 0;
    }
    snapshots.set(toolId, updated);
  }, intervalMs);

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  monitorTimers.set(toolId, timer);
  return snapshot;
}

export function stopMonitoring(toolId: string): void {
  const timer = monitorTimers.get(toolId);
  if (timer) {
    clearInterval(timer);
    monitorTimers.delete(toolId);
  }
}

export function stopAllMonitoring(): void {
  for (const [toolId] of monitorTimers) {
    stopMonitoring(toolId);
  }
}

export function getUnhealthyTools(): HealthSnapshot[] {
  return Array.from(snapshots.values()).filter((s) => s.status === "offline" || s.status === "degraded");
}
