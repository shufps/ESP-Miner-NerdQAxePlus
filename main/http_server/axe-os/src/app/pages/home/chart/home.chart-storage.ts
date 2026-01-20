import type { PersistedHomeChartStateV1, HomeChartViewMode } from './home.chart-state';
import { HOME_CFG } from '../home.cfg';

/**
 * Storage wrapper for Home chart persistence.
 *
 * Step 6 additions:
 * - versioned envelope for the main chartData payload
 * - best-effort migrations from older shapes
 * - light validation / normalization to avoid "poisoned" localStorage
 */

export interface StorageAdapter {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
}

export interface HomeChartStorageKeys {
  chartData: string;
  lastTimestamp: string;
  viewMode: string;
  legendVisibility: string;
  minHistoryTimestampMs: string;
}

export const DEFAULT_HOME_CHART_STORAGE_KEYS: HomeChartStorageKeys = {
  chartData: HOME_CFG.storage.keys.chartData,
  lastTimestamp: HOME_CFG.storage.keys.lastTimestamp,
  viewMode: HOME_CFG.storage.keys.viewMode,
  legendVisibility: HOME_CFG.storage.keys.legendVisibility,
  minHistoryTimestampMs: HOME_CFG.storage.keys.minHistoryTimestampMs,
};

const CURRENT_ENVELOPE_VERSION = 2 as const;
const MAX_POINTS_PERSISTED = HOME_CFG.storage.maxPersistedPoints;

type PersistedEnvelopeV2 = {
  v: typeof CURRENT_ENVELOPE_VERSION;
  ts: number; // persisted-at timestamp (ms)
  state: PersistedHomeChartStateV1;
};

function isNumArray(v: any): v is number[] {
  return Array.isArray(v);
}

function toNumberArray(v: any): number[] | null {
  if (!Array.isArray(v)) return null;
  return v.map((x) => {
    const n = Number(x);
    return Number.isFinite(n) ? n : Number.NaN;
  });
}

function normalizeSeriesLen(arr: number[] | null, len: number): number[] {
  if (!arr) return new Array(len).fill(Number.NaN);
  if (arr.length === len) return arr;
  if (arr.length > len) return arr.slice(0, len);
  return arr.concat(new Array(len - arr.length).fill(Number.NaN));
}

function clampPersistedState(state: PersistedHomeChartStateV1): PersistedHomeChartStateV1 {
  const labels = toNumberArray((state as any).labels) ?? [];

  // enforce cap from the end (keep most recent)
  const cap = Math.min(labels.length, MAX_POINTS_PERSISTED);
  const start = labels.length > cap ? labels.length - cap : 0;
  const labelsCapped = labels.slice(start);

  const len = labelsCapped.length;

  const dataData1m = normalizeSeriesLen(toNumberArray((state as any).dataData1m), labels.length).slice(start);
  const dataData10m = normalizeSeriesLen(toNumberArray((state as any).dataData10m), labels.length).slice(start);
  const dataData1h = normalizeSeriesLen(toNumberArray((state as any).dataData1h), labels.length).slice(start);
  const dataData1d = normalizeSeriesLen(toNumberArray((state as any).dataData1d), labels.length).slice(start);

  const dataVregTemp = normalizeSeriesLen(toNumberArray((state as any).dataVregTemp), labels.length).slice(start);
  const dataAsicTemp = normalizeSeriesLen(toNumberArray((state as any).dataAsicTemp), labels.length).slice(start);

  // Basic sanity: required series must exist and match labels
  if (len === 0) {
    return {
      schema: 1,
      labels: [],
      dataData1m: [],
      dataData10m: [],
      dataData1h: [],
      dataData1d: [],
      dataVregTemp: [],
      dataAsicTemp: [],
    };
  }

  return {
    schema: 1,
    labels: labelsCapped,
    dataData1m,
    dataData10m,
    dataData1h,
    dataData1d,
    dataVregTemp,
    dataAsicTemp,
  };
}

function tryParseJson(raw: string): any | null {
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

function migrateToStateV1(parsed: any): PersistedHomeChartStateV1 | null {
  if (!parsed || typeof parsed !== 'object') return null;

  // v2 envelope
  if (parsed.v === CURRENT_ENVELOPE_VERSION && parsed.state && typeof parsed.state === 'object') {
    return clampPersistedState(parsed.state as PersistedHomeChartStateV1);
  }

  // v1 bare state (schema=1)
  if ((parsed.schema === 1 || parsed.schema == null) && isNumArray(parsed.labels)) {
    // accept missing schema as v0-ish, we’ll normalize below
    const hasRequired =
      isNumArray(parsed.dataData1m) &&
      isNumArray(parsed.dataData10m) &&
      isNumArray(parsed.dataData1h) &&
      isNumArray(parsed.dataData1d);

    if (!hasRequired) return null;

    const v1: PersistedHomeChartStateV1 = {
      schema: 1,
      labels: parsed.labels,
      dataData1m: parsed.dataData1m,
      dataData10m: parsed.dataData10m,
      dataData1h: parsed.dataData1h,
      dataData1d: parsed.dataData1d,
      dataVregTemp: parsed.dataVregTemp,
      dataAsicTemp: parsed.dataAsicTemp,
    };

    return clampPersistedState(v1);
  }

  return null;
}

export class HomeChartStorage {
  constructor(
    private readonly storage: StorageAdapter,
    public readonly keys: HomeChartStorageKeys = DEFAULT_HOME_CHART_STORAGE_KEYS,
  ) {}

  /**
   * Returns a migrated + normalized v1 state (the component/state loader already accepts v1).
   * If the stored payload is garbage/unknown, returns null.
   */
  loadPersistedState(): PersistedHomeChartStateV1 | null {
    const raw = this.storage.getItem(this.keys.chartData);
    if (!raw) return null;

    const parsed = tryParseJson(raw);
    if (!parsed) return null;

    const migrated = migrateToStateV1(parsed);
    if (!migrated) return null;

    return migrated;
  }

  /**
   * Persist using a versioned envelope. Older readers can still read `state` via migration.
   */
  savePersistedState(state: PersistedHomeChartStateV1): void {
    try {
      const normalized = clampPersistedState({ schema: 1, ...state });
      const payload: PersistedEnvelopeV2 = {
        v: CURRENT_ENVELOPE_VERSION,
        ts: Date.now(),
        state: normalized,
      };
      this.storage.setItem(this.keys.chartData, JSON.stringify(payload));
    } catch {
      // ignore (quota/privacy)
    }
  }

  clearPersistedState(): void {
    try {
      this.storage.removeItem(this.keys.chartData);
    } catch {}
  }

  loadLastTimestamp(): number | null {
    const raw = this.storage.getItem(this.keys.lastTimestamp);
    if (!raw) return null;
    const n = Number(raw);
    return Number.isFinite(n) ? n : null;
  }

  saveLastTimestamp(timestampMs: number): void {
    if (!Number.isFinite(timestampMs)) return;
    try {
      this.storage.setItem(this.keys.lastTimestamp, String(Math.trunc(timestampMs)));
    } catch {}
  }

  clearLastTimestamp(): void {
    try {
      this.storage.removeItem(this.keys.lastTimestamp);
    } catch {}
  }

  loadLegendVisibility(): boolean[] | null {
    const raw = this.storage.getItem(this.keys.legendVisibility);
    if (!raw) return null;
    try {
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed)) return null;
      return parsed.map((x) => !!x);
    } catch {
      return null;
    }
  }

  saveLegendVisibility(hiddenFlags: boolean[]): void {
    try {
      this.storage.setItem(this.keys.legendVisibility, JSON.stringify(hiddenFlags.map(Boolean)));
    } catch {}
  }

  loadViewMode(): HomeChartViewMode | null {
    const v = this.storage.getItem(this.keys.viewMode);
    return v === 'gauge' || v === 'bars' ? (v as HomeChartViewMode) : null;
  }

  saveViewMode(mode: HomeChartViewMode): void {
    try {
      this.storage.setItem(this.keys.viewMode, mode);
    } catch {}
  }

  loadMinHistoryTimestampMs(): number | null {
    const raw = this.storage.getItem(this.keys.minHistoryTimestampMs);
    if (!raw) return null;
    const n = Number(raw);
    return Number.isFinite(n) ? n : null;
  }

  saveMinHistoryTimestampMs(ts: number): void {
    if (!Number.isFinite(ts) || ts <= 0) return;
    try {
      this.storage.setItem(this.keys.minHistoryTimestampMs, String(Math.trunc(ts)));
    } catch {}
  }

  clearMinHistoryTimestampMs(): void {
    try {
      this.storage.removeItem(this.keys.minHistoryTimestampMs);
    } catch {}
  }
}
