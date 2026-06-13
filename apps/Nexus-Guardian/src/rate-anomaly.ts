export interface RateTracker {
  name: string;
  windowMs: number;
  bucketMs: number;
  buckets: RateBucket[];
  baseline: { mean: number; stdDev: number } | null;
  totalSamples: number;
}

export interface RateBucket {
  timestamp: number;
  count: number;
}

export interface RateAnomaly {
  name: string;
  currentRate: number;
  baselineMean: number;
  baselineStdDev: number;
  deviationStd: number;
  severity: "low" | "medium" | "high" | "critical";
  timestamp: string;
}

const trackers = new Map<string, RateTracker>();
const anomalies: RateAnomaly[] = [];
const maxAnomalies = 500;

function getOrCreateTracker(name: string, windowMs = 300000, bucketMs = 10000): RateTracker {
  const existing = trackers.get(name);
  if (existing) return existing;

  const tracker: RateTracker = {
    name,
    windowMs,
    bucketMs,
    buckets: [],
    baseline: null,
    totalSamples: 0,
  };
  trackers.set(name, tracker);
  return tracker;
}

export function recordRate(name: string, count = 1): RateAnomaly | null {
  const tracker = getOrCreateTracker(name);
  const now = Date.now();
  const bucketStart = now - (now % tracker.bucketMs);

  let bucket = tracker.buckets.find((b) => b.timestamp === bucketStart);
  if (!bucket) {
    bucket = { timestamp: bucketStart, count: 0 };
    tracker.buckets.push(bucket);
  }
  bucket.count += count;
  tracker.totalSamples += count;

  // Prune old buckets
  const cutoff = now - tracker.windowMs;
  tracker.buckets = tracker.buckets.filter((b) => b.timestamp >= cutoff);

  // Need at least 10 buckets for statistical significance
  if (tracker.buckets.length < 10) return null;

  // Calculate baseline (mean and std dev)
  const counts = tracker.buckets.map((b) => b.count);
  const sum = counts.reduce((a, b) => a + b, 0);
  const mean = sum / counts.length;
  const variance = counts.reduce((s, c) => s + (c - mean) ** 2, 0) / counts.length;
  const stdDev = Math.sqrt(variance);

  tracker.baseline = { mean, stdDev: stdDev || 1 };

  // Check current bucket
  const currentCount = bucket.count;
  const deviation = stdDev > 0 ? (currentCount - mean) / stdDev : 0;

  if (deviation <= 2) return null;

  const severity: RateAnomaly["severity"] =
    deviation > 5 ? "critical" : deviation > 4 ? "high" : deviation > 3 ? "medium" : "low";

  const anomaly: RateAnomaly = {
    name,
    currentRate: currentCount,
    baselineMean: Math.round(mean * 10) / 10,
    baselineStdDev: Math.round(stdDev * 10) / 10,
    deviationStd: Math.round(deviation * 10) / 10,
    severity,
    timestamp: new Date().toISOString(),
  };

  anomalies.push(anomaly);
  if (anomalies.length > maxAnomalies) anomalies.shift();

  return anomaly;
}

export function getRateTracker(name: string): RateTracker | undefined {
  return trackers.get(name);
}

export function listRateTrackers(): RateTracker[] {
  return Array.from(trackers.values());
}

export function getRateAnomalies(limit = 50): RateAnomaly[] {
  return anomalies.slice(-limit).reverse();
}

export function clearRateData(): void {
  trackers.clear();
  anomalies.length = 0;
}
