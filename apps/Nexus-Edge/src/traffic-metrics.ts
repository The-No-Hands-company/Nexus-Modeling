import type { TrafficMetric, TrafficSummary } from "./types";

const metrics: TrafficMetric[] = [];
const maxMetrics = 10000;

export function recordMetric(metric: Omit<TrafficMetric, "timestamp">): void {
  metrics.push({ ...metric, timestamp: new Date().toISOString() });
  if (metrics.length > maxMetrics) {
    metrics.splice(0, metrics.length - maxMetrics);
  }
}

export function getMetrics(limit = 100): TrafficMetric[] {
  return metrics.slice(-limit).reverse();
}

export function getMetricsForHost(host: string, limit = 100): TrafficMetric[] {
  return metrics
    .filter((m) => m.host === host)
    .slice(-limit)
    .reverse();
}

export function getTrafficSummary(): TrafficSummary[] {
  const summaries = new Map<string, {
    totalRequests: number;
    totalLatency: number;
    statusCodes: Record<number, number>;
    lastRequestAt: string;
  }>();

  for (const m of metrics) {
    const existing = summaries.get(m.host) || {
      totalRequests: 0,
      totalLatency: 0,
      statusCodes: {},
      lastRequestAt: m.timestamp,
    };

    existing.totalRequests++;
    existing.totalLatency += m.latencyMs;
    existing.statusCodes[m.statusCode] = (existing.statusCodes[m.statusCode] || 0) + 1;
    existing.lastRequestAt = m.timestamp;

    summaries.set(m.host, existing);
  }

  const results: TrafficSummary[] = [];
  for (const [host, data] of summaries) {
    results.push({
      host,
      totalRequests: data.totalRequests,
      avgLatencyMs: Math.round(data.totalLatency / data.totalRequests),
      statusCodes: data.statusCodes,
      lastRequestAt: data.lastRequestAt,
    });
  }

  return results.sort((a, b) => b.totalRequests - a.totalRequests);
}

export function clearMetrics(): void {
  metrics.length = 0;
}
