// Debug hooks for the Nerd charts dashboard.
// Goal: keep all global/window wiring out of the Angular component.
// This file should not contain chart logic (axis maths, GraphGuard, etc.) — only wiring.

export interface SafeStorage {
  getItem: (key: string) => string | null;
  setItem: (key: string, value: string) => void;
  removeItem: (key: string) => void;
}

export interface NerdChartsDebugDeps {
  storage: SafeStorage;

  // Core actions (implemented by the component)
  clearChartHistoryInternal: (updateChartNow: boolean) => void;
  setAxisPadding: (cfg: any, persist: boolean) => void;
  saveAxisPaddingOverrides: () => void;
  disableAxisPaddingOverride: () => void;
  setHashrateTicks: (n: number) => void;
  setHashrateMinTickStep: (ths: number) => void;
  dumpAxisScale: () => any;
  flushHistoryDrainRender: () => void;

  // System actions (optional)
  restart?: (totp?: string) => any;
}

export interface NerdChartsDebugKeys {
  debugModeKey?: string;
  clearOnceKey?: string;
  forceStartTimestampKey?: string;
  minHistoryTimestampKey?: string;
  lastTimestampKey?: string;
}

const DEFAULT_KEYS: Required<NerdChartsDebugKeys> = {
  debugModeKey: '__nerdCharts_debugMode',
  clearOnceKey: '__nerdCharts_clearChartHistoryOnce',
  forceStartTimestampKey: '__nerdCharts_forceStartTimestampMs',
  minHistoryTimestampKey: '__nerdCharts_minHistoryTimestampMs',
  lastTimestampKey: 'lastTimestamp',
};

function safeNowMs(): number {
  return Date.now();
}

function getKeys(keys?: NerdChartsDebugKeys): Required<NerdChartsDebugKeys> {
  return { ...DEFAULT_KEYS, ...(keys || {}) };
}

function ensureRoot(globalObj: any): any {
  const g: any = globalObj as any;
  const existing = (g.__nerdCharts && typeof g.__nerdCharts === 'object') ? g.__nerdCharts : {};
  g.__nerdCharts = existing;
  return existing;
}

export function installNerdChartsDebugHooks(globalObj: any, deps: NerdChartsDebugDeps, keys?: NerdChartsDebugKeys): void {
  const k = getKeys(keys);
  const obj = ensureRoot(globalObj);

  // Clear all in-browser chart history once (persisted + in-memory on next load)
  obj.clearChartHistoryOnce = () => {
    try {
      deps.storage.setItem(k.clearOnceKey, '1');

      // Ensure the next load does NOT immediately re-fill from API history (debug helper)
      const seed = safeNowMs() - 30_000;
      deps.storage.setItem(k.forceStartTimestampKey, String(seed));
      deps.storage.setItem(k.minHistoryTimestampKey, String(seed));
      deps.storage.setItem(k.lastTimestampKey, String(seed));
      return { ok: true, seed };
    } catch (e: any) {
      // eslint-disable-next-line no-console
      console.warn('[nerdCharts] clearChartHistoryOnce failed', e);
      return { ok: false, error: String(e) };
    }
  };

  // Clear immediately (no reload needed)
  obj.clearChartHistoryNow = () => {
    try {
      deps.clearChartHistoryInternal(true);
      return { ok: true };
    } catch (e: any) {
      // eslint-disable-next-line no-console
      console.warn('[nerdCharts] clearChartHistoryNow failed', e);
      return { ok: false, error: String(e) };
    }
  };

  // Debug: tweak adaptive axis padding via console
  obj.setAxisPadding = (cfg: any) => {
    const persist = !!(cfg && typeof cfg === 'object' && cfg.persist === true);
    deps.setAxisPadding(cfg, persist);
  };

  obj.saveAxisPadding = () => deps.saveAxisPaddingOverrides();

  obj.disableAxisPaddingOverride = () => deps.disableAxisPaddingOverride();

  obj.setHashrateTicks = (n: any) => {
    const v = Math.round(Number(n));
    if (!Number.isFinite(v)) return;
    deps.setHashrateTicks(v);
  };

  obj.setHashrateMinTickStep = (ths: any) => {
    const v = Number(ths);
    if (!Number.isFinite(v) || v <= 0) return;
    deps.setHashrateMinTickStep(v);
  };

  obj.dumpAxisScale = () => deps.dumpAxisScale();

  obj.flushHistoryDrainRender = () => deps.flushHistoryDrainRender();

  // Device restart (calls backend restart endpoint). Optional so existing builds don't break.
  obj.restart = (totp?: string) => {
    try {
      if (typeof deps.restart !== 'function') {
        return { ok: false, error: 'restart hook not installed' };
      }
      return deps.restart(totp);
    } catch (e: any) {
      // eslint-disable-next-line no-console
      console.warn('[nerdCharts] restart failed', e);
      return { ok: false, error: String(e) };
    }
  };

  obj.list = () => Object.keys(obj).sort();

  obj.help = () => ({
    bootstrap: {
      enable: 'enable(persist?: boolean)',
      disable: 'disable(clearPersist?: boolean)',
    },
    history: {
      clearChartHistoryNow: 'clearChartHistoryNow()',
      clearChartHistoryOnce: 'clearChartHistoryOnce(); location.reload()',
      flushHistoryDrainRender: 'flushHistoryDrainRender()',
    },
    system: {
      restart: 'restart(totp?: string)',
    },
    axes: {
      setAxisPadding: 'setAxisPadding({ hashPadPctTop?, hashPadPctBottom?, hashMinPadThs?, hashFlatPadPctOfMax?, hashMaxPadPctOfMax?, tempPadPct?, tempMinPadC?, debug?, persist? })',
      saveAxisPadding: 'saveAxisPadding()',
      disableAxisPaddingOverride: 'disableAxisPaddingOverride(); location.reload()',
      setHashrateTicks: 'setHashrateTicks(n: number)',
      setHashrateMinTickStep: 'setHashrateMinTickStep(ths: number)',
      dumpAxisScale: 'dumpAxisScale()',
    },
  });
}

export function installNerdChartsDebugBootstrap(globalObj: any, deps: NerdChartsDebugDeps, keys?: NerdChartsDebugKeys): void {
  const k = getKeys(keys);
  const obj = ensureRoot(globalObj);

  if (typeof obj.enable !== 'function') {
    obj.enable = (persist?: boolean) => {
      try { if (persist) deps.storage.setItem(k.debugModeKey, '1'); } catch {}
      if (!obj.__enabled) {
        installNerdChartsDebugHooks(globalObj, deps, keys);
        obj.__enabled = true;
      }
      return obj;
    };
  }

  if (typeof obj.disable !== 'function') {
    obj.disable = (clearPersist?: boolean) => {
      try { if (clearPersist) deps.storage.removeItem(k.debugModeKey); } catch {}
      return obj;
    };
  }

  if (typeof obj.help !== 'function') {
    obj.help = () => ({
      enable: 'enable(persist?: boolean)',
      disable: 'disable(clearPersist?: boolean)',
      note: 'Call enable() to install full debug hooks. If persist is true, debug mode will auto-enable after reload.',
    });
  }

  // Auto-enable if persisted.
  const autoEnabled = (() => {
    try { return deps.storage.getItem(k.debugModeKey) === '1'; } catch { return false; }
  })();

  if (autoEnabled) obj.enable(false);
}
