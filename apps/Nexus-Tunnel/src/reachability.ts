import type { ReachabilityRecord } from "./types";

const reachability = new Map<string, ReachabilityRecord>();

export async function checkReachability(toolId: string, targetUrl: string): Promise<ReachabilityRecord> {
  const start = Date.now();
  let reachable = false;
  let responseTimeMs = 0;
  let errorMessage: string | undefined;

  try {
    const response = await fetch(targetUrl, {
      method: "HEAD",
      signal: AbortSignal.timeout(5000),
    });
    responseTimeMs = Date.now() - start;
    reachable = response.ok || response.status < 500;
  } catch (err) {
    responseTimeMs = Date.now() - start;
    errorMessage = (err as Error).message;
  }

  const existing = reachability.get(toolId);
  const consecutiveFailures = reachable ? 0 : (existing?.consecutiveFailures || 0) + 1;

  const record: ReachabilityRecord = {
    toolId,
    targetUrl,
    lastReachable: reachable ? new Date().toISOString() : existing?.lastReachable || "",
    lastChecked: new Date().toISOString(),
    consecutiveFailures,
    reachable,
    responseTimeMs,
    errorMessage,
  };

  reachability.set(toolId, record);
  return record;
}

export function getReachability(toolId: string): ReachabilityRecord | undefined {
  return reachability.get(toolId);
}

export function listReachability(): ReachabilityRecord[] {
  return Array.from(reachability.values());
}

export function isReachable(toolId: string): boolean {
  return reachability.get(toolId)?.reachable ?? false;
}

export function startReachabilityChecks(
  toolIds: string[],
  getTargetUrl: (toolId: string) => string | undefined,
  intervalMs: number,
): () => void {
  const checkAll = async () => {
    for (const toolId of toolIds) {
      const targetUrl = getTargetUrl(toolId);
      if (!targetUrl) continue;
      await checkReachability(toolId, targetUrl);
    }
  };

  checkAll();

  const timer = setInterval(checkAll, intervalMs);

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}

export function clearReachability(): void {
  reachability.clear();
}
