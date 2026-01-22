/**
 * ChartState: pure in-memory time-series state for the Home chart.
 *
 * Goals:
 * - no Angular dependencies
 * - no Chart.js dependencies
 * - stable, versioned persisted schema
 */

import { HOME_CFG } from '../home.cfg';

export interface PersistedHomeChartStateV1 {
  schema?: 1;
  labels: number[];
  dataData1m: number[];
  dataData10m: number[];
  dataData1h: number[];
  dataData1d: number[];
  dataVregTemp?: number[];
  dataAsicTemp?: number[];
}


export type HomeChartViewMode = 'gauge' | 'bars';
export interface AppendHistoryOptions {
  /**
   * If set, discard any points with timestamps lower than this.
   * Useful after a user-triggered "clear history" to prevent immediate refill.
   */
  minTimestampMs?: number | null;
}

/**
 * Minimal view of the API history chunk this component currently consumes.
 * (Kept loose for backward compatibility.)
 */
export interface ApiHistoryChunk {
  timestamps?: unknown;
  timestampBase?: unknown;
  hashrate_1m?: unknown;
  hashrate_10m?: unknown;
  hashrate_1h?: unknown;
  hashrate_1d?: unknown;
  vregTemp?: unknown;
  asicTemp?: unknown;
}

function isNumberArray(v: any): v is number[] {
  return Array.isArray(v) && v.every((x) => typeof x === 'number');
}

function normalizeToLen(arr: number[] | null | undefined, len: number): number[] {
  if (!arr) return new Array(len).fill(Number.NaN);
  if (arr.length === len) return arr;
  if (arr.length > len) return arr.slice(0, len);
  return arr.concat(new Array(len - arr.length).fill(Number.NaN));
}

function toAbsMs(tsBase: number, rel: number): number {
  let ts = Number(tsBase) + Number(rel);
  // accept both seconds and milliseconds
  if (ts > 0 && ts < 1_000_000_000_000) ts *= 1000;
  return ts;
}

export class HomeChartState {
  labels: number[] = [];
  hr1m: number[] = [];
  hr10m: number[] = [];
  hr1h: number[] = [];
  hr1d: number[] = [];
  vregTemp: number[] = [];
  asicTemp: number[] = [];

  clear(): void {
    this.labels = [];
    this.hr1m = [];
    this.hr10m = [];
    this.hr1h = [];
    this.hr1d = [];
    this.vregTemp = [];
    this.asicTemp = [];
  }

  /**
   * Ensure all series match labels length. If repair is not possible, resets in-memory.
   * Returns true if state is usable after the call.
   */
  validateLengthsOrReset(): boolean {
    const lenLabels = this.labels.length;
    const lens = [
      lenLabels,
      this.hr1m.length,
      this.hr10m.length,
      this.hr1h.length,
      this.hr1d.length,
      this.vregTemp.length,
      this.asicTemp.length,
    ];

    const allEqual = lens.every((l) => l === lens[0]);
    if (allEqual) return true;

    const anySeriesHasData = lens.slice(1).some((l) => l > 0);
    if (lenLabels === 0 && anySeriesHasData) {
      // Can't recover: labels missing but series present.
      this.clear();
      return false;
    }

    // Repair: normalize all series to labels length.
    this.hr1m = normalizeToLen(this.hr1m, lenLabels);
    this.hr10m = normalizeToLen(this.hr10m, lenLabels);
    this.hr1h = normalizeToLen(this.hr1h, lenLabels);
    this.hr1d = normalizeToLen(this.hr1d, lenLabels);
    this.vregTemp = normalizeToLen(this.vregTemp, lenLabels);
    this.asicTemp = normalizeToLen(this.asicTemp, lenLabels);
    return true;
  }

  /** Trim series to keep only points newer than cutoffMs. */
private trimToCutoff(cutoffMs: number): void {
  const len = this.labels.length;
  if (!len) return;

  let idx = 0;
  while (idx < len && this.labels[idx] < cutoffMs) idx++;
  if (idx <= 0) return;

  this.labels = this.labels.slice(idx);
  this.hr1m = this.hr1m.slice(idx);
  this.hr10m = this.hr10m.slice(idx);
  this.hr1h = this.hr1h.slice(idx);
  this.hr1d = this.hr1d.slice(idx);
  this.vregTemp = this.vregTemp.slice(idx);
  this.asicTemp = this.asicTemp.slice(idx);

}

/**
 * Trim series to the last `windowMs` relative to `nowMs`.
 *
 * `windowMs` defaults to HOME_CFG.xAxis.fixedWindowMs so callers don't need
 * to thread config through multiple layers.
 */
trimToWindow(nowMs: number, windowMs: number = HOME_CFG.xAxis.fixedWindowMs): void {
  const w = Math.max(0, Math.round(Number(windowMs) || 0));
  this.trimToCutoff(nowMs - w);
}

  /**
   * Convert in-memory state to a persisted, versioned schema.
   * (Keep field names stable for backward compatibility.)
   */
  toPersisted(): PersistedHomeChartStateV1 {
    return {
      schema: 1,
      labels: this.labels,
      dataData1m: this.hr1m,
      dataData10m: this.hr10m,
      dataData1h: this.hr1h,
      dataData1d: this.hr1d,
      dataVregTemp: this.vregTemp,
      dataAsicTemp: this.asicTemp,
    };
  }

  /**
   * Restore from persisted schema.
   * Accepts older shapes (missing temp series) and normalizes lengths.
   */
  static fromPersisted(raw: unknown): HomeChartState {
    const parsed: any = raw ?? {};
    const labels = Array.isArray(parsed?.labels) ? parsed.labels.map(Number) : null;

    const d1m = Array.isArray(parsed?.dataData1m) ? parsed.dataData1m.map(Number) : null;
    const d10m = Array.isArray(parsed?.dataData10m) ? parsed.dataData10m.map(Number) : null;
    const d1h = Array.isArray(parsed?.dataData1h) ? parsed.dataData1h.map(Number) : null;
    const d1d = Array.isArray(parsed?.dataData1d) ? parsed.dataData1d.map(Number) : null;

    if (!labels || !d1m || !d10m || !d1h || !d1d) {
      throw new Error('Invalid persisted chart state (missing required series)');
    }

    const targetLen = labels.length;
    const vreg = Array.isArray(parsed?.dataVregTemp) ? parsed.dataVregTemp.map(Number) : null;
    const asic = Array.isArray(parsed?.dataAsicTemp) ? parsed.dataAsicTemp.map(Number) : null;

    const s = new HomeChartState();
    s.labels = labels;
    s.hr1m = normalizeToLen(d1m, targetLen);
    s.hr10m = normalizeToLen(d10m, targetLen);
    s.hr1h = normalizeToLen(d1h, targetLen);
    s.hr1d = normalizeToLen(d1d, targetLen);
    s.vregTemp = normalizeToLen(vreg, targetLen);
    s.asicTemp = normalizeToLen(asic, targetLen);
    s.validateLengthsOrReset();
    return s;
  }

  /**
   * Append a history chunk (API format). This does *not* apply spike-guards/heuristics.
   * It only performs timestamp conversion + dedupe.
   *
   * The component (or a higher-level pipeline) can post-process values (GraphGuard) before calling this.
   */
  appendHistoryChunk(chunk: ApiHistoryChunk, opts: AppendHistoryOptions = {}): number {
    const tsArr = Array.isArray(chunk?.timestamps) ? (chunk.timestamps as any[]).map(Number) : [];
    const base = Number((chunk as any)?.timestampBase ?? 0);

    const h1m = Array.isArray(chunk?.hashrate_1m) ? (chunk.hashrate_1m as any[]).map(Number) : [];
    const h10m = Array.isArray(chunk?.hashrate_10m) ? (chunk.hashrate_10m as any[]).map(Number) : [];
    const h1h = Array.isArray(chunk?.hashrate_1h) ? (chunk.hashrate_1h as any[]).map(Number) : [];
    const h1d = Array.isArray(chunk?.hashrate_1d) ? (chunk.hashrate_1d as any[]).map(Number) : [];
    const vreg = Array.isArray(chunk?.vregTemp) ? (chunk.vregTemp as any[]).map(Number) : [];
    const asic = Array.isArray(chunk?.asicTemp) ? (chunk.asicTemp as any[]).map(Number) : [];

    const n = Math.min(tsArr.length, h1m.length, h10m.length, h1h.length, h1d.length, vreg.length, asic.length);
    if (n <= 0) return 0;

    const lastTs = this.labels.length ? this.labels[this.labels.length - 1] : -Infinity;
    const minTs = opts.minTimestampMs ?? null;

    let appended = 0;
    for (let i = 0; i < n; i++) {
      const tsAbs = toAbsMs(base, tsArr[i]);
      if (!Number.isFinite(tsAbs)) continue;
      if (minTs != null && tsAbs < minTs) continue;
      if (tsAbs < lastTs) continue;

      const lastIdx = this.labels.length - 1;
      if (lastIdx >= 0 && this.labels[lastIdx] === tsAbs) {
        // overwrite duplicate timestamp (prevents vertical line segments)
        this.hr1m[lastIdx] = h1m[i];
        this.hr10m[lastIdx] = h10m[i];
        this.hr1h[lastIdx] = h1h[i];
        this.hr1d[lastIdx] = h1d[i];
        this.vregTemp[lastIdx] = vreg[i];
        this.asicTemp[lastIdx] = asic[i];
        continue;
      }

      this.labels.push(tsAbs);
      this.hr1m.push(h1m[i]);
      this.hr10m.push(h10m[i]);
      this.hr1h.push(h1h[i]);
      this.hr1d.push(h1d[i]);
      this.vregTemp.push(vreg[i]);
      this.asicTemp.push(asic[i]);
      appended++;
    }

    return appended;
  }
}
