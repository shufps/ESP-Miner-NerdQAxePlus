/**
 * Central config for the *Experimental* dashboard.
 *
 * This config is intentionally scoped to the experimental dashboard implementation.
 * The decision "legacy vs experimental" is controlled by the UI settings via
 * ExperimentalDashboardService.enabled$ (HomeShellComponent) — not from this file.
 */

import type { HomeChartViewMode } from './chart/home.chart-state';


// ---- UI defaults

/**
 * Default legend visibility flags. `true` means "hidden" (Chart.js convention).
 *
 * If you want a dataset hidden by default, set its key to true.
 */
export interface HomeLegendHiddenDefaults {
  hr1m?: boolean;
  hr10m?: boolean;
  hr1h?: boolean;
  hr1d?: boolean;
  vregTemp?: boolean;
  asicTemp?: boolean;
}


// ---- Axis scaling & ticks

/**
 * Axis padding configuration so lines don't visually stick to the chart frame.
 */
export interface AxisPaddingCfg {
  // Hashrate axis padding rules.
  hashrate: {
    /** How many last points to consider when computing the current range. */
    windowPoints: number;
    /** Fallback symmetric padding as % of range. */
    padPct: number;
    /** Extra headroom above range (as % of range). */
    padPctTop: number;
    /** Extra room below range (as % of range). */
    padPctBottom: number;
    /** Minimum padding in TH/s (keeps flat curves from looking stuck). */
    minPadThs: number;
    /** If range ~0, use max*X as padding. */
    flatPadPctOfMax: number;
    /** Cap padding to avoid zooming out too much. */
    maxPadPctOfMax: number;
  };
  // Temperature axis padding rules.
  temp: {
    windowPoints: number;
    padPct: number;
    minPadC: number;
    /** If range ~0, use a fixed padding (in °C). */
    flatPadC: number;
    /** Cap padding to avoid zooming out too much. */
    maxPadC: number;
  };
}

/**
 * Y-axis tick label count clamp.
 * Prevents accidental crazy values from debug inputs.
 */
export interface TickCountClamp {
  min: number;
  max: number;
}

/**
 * Minimum tick step sizes.
 * Use these to avoid jittery axis labels when the range is tiny.
 */
export interface MinTickSteps {
  /** Minimum hashrate axis step in TH/s. */
  hashrateMinStepThs: number;
  /** Minimum temperature axis step in °C. */
  tempMinStepC: number;
}


// ---- GraphGuard

/**
 * Per-series relative thresholds for the GraphGuard.
 *
 * `relThreshold` is the relative delta (vs previous value) above which a sample
 * is treated as "suspicious" and may be blocked until confirmed.
 */
export interface GraphGuardThresholds {
  hashrate1m: number;
  hashrate10m: number;
  hashrate1h: number;
  hashrate1d: number;
  vregTemp: number;
  asicTemp: number;
}

/**
 * GraphGuard core behavior knobs.
 */
export interface GraphGuardCfg {
  /** How many consecutive suspicious samples are required to confirm a step. */
  confirmSamples: number;
  /**
   * If a suspicious hashrate step matches the live reference within this tolerance,
   * accept immediately (fast scale reaction).
   */
  liveRefTolerance: number;
  /** Steps >= this relative delta vs previous sample are treated as real changes. */
  bigStepRel: number;
  /** Live-ref stability detector: number of samples needed. */
  liveRefStableSamples: number;
  /** Live-ref stability detector: max relative variance allowed. */
  liveRefStableRel: number;
}

/**
 * Wrapper for GraphGuard-related configuration.
 */
export interface GraphGuardTuning {
  /** Global GraphGuard behavior. */
  cfg: GraphGuardCfg;
  /** Per-series thresholds (relThreshold). */
  thresholds: GraphGuardThresholds;
  /** Enable guarding for hashrate series (temps are always guarded). */
  enableHashrateSpikeGuard: boolean;
}


// ---- History drain

/**
 * Controls history draining behavior (chunked history fetching).
 */
export interface HistoryDrainCfg {
  /**
   * Render throttling in ms while draining (reduces UI jank).
   * Lower -> more frequent updates; Higher -> smoother but less "live" feeling.
   */
  renderThrottleMs: number;
  /**
   * If true, render is throttled. If false, each chunk update renders immediately.
   */
  useThrottledRender: boolean;
  /**
   * If true, suppress normal live updates while draining history.
   * This prevents "half-built" states.
   */
  suppressChartUpdatesDuringDrain: boolean;
  /**
   * Chunk size for the history drainer (0 means no limit; drain as API provides).
   */
  chunkSize: number;
}


// ---- Visual smoothing

/**
 * Rendering-only smoothing for the 1m hashrate dataset.
 * Does not change data; only affects curve rendering.
 */
export interface Hashrate1mSmoothingCfg {
  enabled: boolean;
  fastIntervalMs: number; // median point interval thresholds (ms)
  mediumIntervalMs: number;
  tensionFast: number; // tension values (0..1)
  tensionMedium: number;
  tensionSlow: number;
  cubicInterpolationMode: 'monotone' | 'default'; // interpolation mode (Chart.js)
  /**
   * How many last label deltas are considered when computing the median interval.
   * (clamped internally to available points)
   */
  medianWindowPoints: number;
  /** Extra tension to add per zoom step (visual-only). */
  zoomBoostPerStep: number;
  /**
   * EMA smoothing window target in ms (converted to points via median interval).
   * These values are only used when zoomed out (per zoom step).
   */
  emaWindowMsMin: number;
  emaWindowMsMax: number;
  emaWindowMsPerStep: number;
  /** Clamp the EMA window (in points) after conversion. */
  emaMinPoints: number;
  emaMaxPoints: number;
  /** Ensure the last point stays precise (reduces perceived lag). */
  snapLastPoint: boolean;
}


// ---- Temp scaling ("latest" view)

/**
 * Temperature scale defaults for "latest"-style min/max behavior.
 */
export interface TempScaleCfg {
  /**
   * Minimum delta (°C) required before we update the temp axis bounds.
   * This creates a small deadband so the chart doesn't "jump" on tiny changes.
   */
  hysteresisC?: number;
  /**
   * Padding applied to temperature axis bounds (adaptive scaling).
   * Example: axisMinPadC=1, axisMaxPadC=2 => min = mn - 1, max = mx + 2.
   */
  axisMinPadC?: number;
  axisMaxPadC?: number;
}


// ---- Storage

/**
 * Central place for localStorage keys used by the experimental chart.
 *
 */
export interface HomeStorageKeys {
  chartData: string;
  lastTimestamp: string;
  legendVisibility: string;
  viewMode: string;
  minHistoryTimestampMs: string;
  /** Visual-only collapse state for the Home chart container. */
  chartCollapsed: string;
}

/**
 * Defaults for UI state that can be persisted.
 */
export interface HomeUiDefaults {
  viewMode: HomeChartViewMode;
  legendHidden: HomeLegendHiddenDefaults;
}



// ---- Tile / bar / square helpers (Experimental dashboard)

/**
 * Uptime formatting rules used in the Hashrate tile.
 * Kept in config so we can tweak unit cutoffs without touching component code.
 */
export interface UptimeFormatCfg {
  minutesInHour: number;
  minutesInDay: number;
  minutesInWeek: number;
  minutesInMonth: number;
  /** If true, always include minutes (even when days/weeks are shown). */
  alwaysShowMinutes: boolean;
  /** If true, always include seconds (even when hours/days are shown). */
  alwaysShowSeconds: boolean;
}

/**
 * Key/limit mapping used to provide stable aliases for the "Input current" power bar.
 * The backend has seen multiple shapes over firmware versions.
 */
export interface PowerUsageAliasCfg {
  /** Values above this are treated as mA and converted to A. */
  milliAmpThreshold: number;
  /** Fallback max current (A) if nothing can be derived. */
  fallbackMaxA: number;
  /** If max <= min, expand range by this many A. */
  minRangeA: number;

  /** Candidate keys for min current (first finite wins). */
  minKeys: string[];
  /** Candidate keys for max current (first finite wins). */
  maxKeys: string[];
}

/**
 * DOM override helper config.
 * We keep selectors + theme heuristics here so the component stays a thin container.
 */
export interface BarDomSyncCfg {
  /** Query selector for power bars inside the Power Usage tile. */
  powerBarSelector: string;
  /** The unit text that identifies the Input Current bar (e.g. 'A'). */
  currentInputUnit: string;

  /** Query selector for the VR temp bar element. */
  vrTempBarSelector: string;
  /** Query selector for a bar's fill element. */
  fillSelector: string;
  /** Query selector for a bar's label element. */
  labelSelector: string;
  /** Query selector for a bar's value element. */
  valueSelector: string;

  /** Theme-name hints to detect light/dark without hard dependencies. */
  lightThemeHints: string[];
  darkThemeHints: string[];
}

export interface HomeTilesCfg {
  uptime: UptimeFormatCfg;
  powerUsageAliases: PowerUsageAliasCfg;
  domSync: BarDomSyncCfg;

  /** Visual-only thresholds for the Input Current (A) bar. */
  inputCurrent: {
    lowMaxAThreshold?: number;
    lowWarnRel?: number;
    lowCritRel?: number;
    warnRel: number;
    critRel: number;
  };

  /** Visual-only normal band for Input Voltage (V). */
  inputVoltageBand: {
    low: number;
    high: number;
  };

  /** Voltage regulator temperature thresholds in °C (absolute). */
  vrTempBand: {
    warnC: number;
    critC: number;
  };
}
export interface HomeCfg {
  storage: {
    keys: HomeStorageKeys;
    /** Max points persisted (older points are dropped). */
    maxPersistedPoints: number;
  };
  uiDefaults: HomeUiDefaults;
  tiles: HomeTilesCfg;
  axisPadding: AxisPaddingCfg;
  /**
   * Implement X-axis viewport size in milliseconds.
   *
   * Used to keep the plot area visually stable (e.g. always show a full 1h window)
   * even if there are only a few points right after a cold start / localStorage clear.
   */

  colors: {
    /** Grid line color for the chart. Keep it fairly light to reduce visual noise. */
    chartGridColor: '#80808040',
    /** Fallback text color if CSS var is missing (e.g. during early boot). */
    textFallback: '#e5e7eb',

    /** Dataset base colors (centralized so visuals can be tweaked in one place). */
    hashrateBase: '#a564f6',
    vregTemp: '#2DA8B7',
    asicTemp: '#C84847',

    /** Value pills defaults */
    pillsText: '#ffffff',
    /** Debug outline stroke used by the pills plugin (only visible in debug mode). */
    pillsDebugStroke: '#ff00ff',
  },

  xAxis: {
    fixedWindowMs: number;
    /** Minimum zoom window (ms). */
    minWindowMs: number;
    /** Maximum zoom window (ms). */
    maxWindowMs: number;
    /** Zoom step (ms). */
    zoomStepMs: number;
    /** Tick spacing for the time axis (ms). Used for deterministic 15m labels/grid. */
    tickStepMs: number;
  };
  yAxis: {
    hashrateMaxTicksDefault: number;
    hashrateTickCountClamp: TickCountClamp;
    minTickSteps: MinTickSteps;
    /** Max relative expansion to keep other hashrate series visible without rescaling. */
    hashrateSoftIncludeRel: number;
  };
  graphGuard: GraphGuardTuning;
  historyDrain: HistoryDrainCfg;
  smoothing: {
    hashrate1m: Hashrate1mSmoothingCfg;
  };
  tempScale: TempScaleCfg;

  // ---- Warmup / restart gating

  /**
   * Warmup/Restart sequencing so graphs don't "fall apart" after miner restarts.
   *
   * This is used by HomeExperimentalComponent + home.warmup.ts.
   */
  warmup: {
    /** Minimum plausible temperature that counts as "valid" for warmup gating. */
    tempMinValidC: number;
    /** Hard cap for temperatures (sanity). */
    tempMaxValidC: number;

    /** Delay after first valid VR temp sample before enabling VR temp plot. */
    vregDelayMs: number;
    /** Delay after VR is enabled and first valid ASIC temp sample was seen. */
    asicDelayMs: number;
    /** Delay after ASIC is enabled and live hashrate is present before enabling HR 1m plot. */
    hash1mDelayMs: number;

    /** How many consecutive "boot-like" polls are required to treat as restart. */
    restartDetectStreak: number;
  };

  /**
   * Raw-sample sanitizing to avoid impossible values entering the plot.
   * (Invalid samples become NaN -> visual gap)
   */
  sanitize: {
    tempMinC: number;
    tempMaxC: number;
    hashrateMinHs: number;
  };

  /**
   * Startup behavior knobs around GraphGuard.
   */
  startup: {
    /**
     * Number of initial samples (per hashrate series) that bypass GraphGuard
     * once startup is "unlocked" (expected vs live reached).
     */
    bypassGuardSamples: number;
    /** Unlock ratio: live must reach expected*ratio once (per restart) before bypass can be used. */
    expectedUnlockRatio: number;

    /**
     * 1m hashrate GraphGuard behavior:
     * After a restart, we run in a "super smooth" mode for a short window to suppress
     * short-lived dips (e.g. 2–3 ticks). Afterwards, we switch to a snappier mode.
     */
    hr1mSmoothWindowMs: number;
    /** GraphGuard confirmSamples used during the smooth startup window. */
    hr1mConfirmStartup: number;
    /** GraphGuard confirmSamples used after the smooth startup window ("snappier"). */
    hr1mConfirmNormal: number;

    /**
     * Optional: after the 1m smooth window ends, force a single browser reload (F5/Cmd+R).
     * Guarded to trigger ONLY after a miner restart (hard cut) and only once per restart.
     */
    hr1mReloadAfterSmooth: boolean;

    /**
     * Cooldown for the optional auto-reload to avoid reload loops (session-scoped).
     * If the user refreshes manually, we will not auto-reload again until this cooldown passes.
     */
    hr1mReloadCooldownMs: number;

  };
}

/**
 * Returns a deep-cloned axis padding config so runtime debug overrides
 * don't mutate the shared defaults.
 */
export function createAxisPaddingCfg(): AxisPaddingCfg {
  return JSON.parse(JSON.stringify(HOME_CFG.axisPadding)) as AxisPaddingCfg;
}

export const HOME_CFG: HomeCfg = {
  storage: {
    keys: {
      // Keep the existing keys used by the experimental dashboard so upgrades
      // do not silently drop persisted state.
      chartData: 'chartData_exp',
      lastTimestamp: 'lastTimestamp_exp',
      legendVisibility: 'chartLegendVisibility_exp',
      // Historical name was tempViewMode_exp; we call it viewMode in code.
      viewMode: 'tempViewMode_exp',
      minHistoryTimestampMs: 'minHistoryTimestampMs_exp',
      // Visual-only: remember if the user collapsed the chart container.
      chartCollapsed: 'chartCollapsed_exp',
    },
    // Keep storage reasonably bounded; chart can still hold more live points.
    maxPersistedPoints: 20000,
  },

  uiDefaults: {
    // Default view when entering the chart.
    viewMode: 'bars',
    // Default hidden datasets (Chart.js style: hidden=true).
    legendHidden: {
      // Initial defaults (applied only if the user has no persisted legend selection yet).
      hr10m: true,
      hr1h: true,
      hr1d: true,
    },
  },

  tiles: {
    uptime: {
      minutesInHour: 60,
      minutesInDay: 24 * 60,
      minutesInWeek: 7 * 24 * 60,
      // coarse month approximation (matches previous logic)
      minutesInMonth: 30 * 24 * 60,
      alwaysShowMinutes: true,
      alwaysShowSeconds: false,
    },

    powerUsageAliases: {
      milliAmpThreshold: 1000,
      fallbackMaxA: 6,
      minRangeA: 6,
      minKeys: [
        'minCurrentA',
        'minCurrent',
        'currentMin',
        'inputCurrentMin',
        'iinMin',
      ],
      maxKeys: [
        'maxCurrentA',
        'maxCurrent',
        'currentMax',
        'inputCurrentMax',
        'iinMax',
        'inputCurrentLimit',
        'currentLimit',
        'iinLimit',
      ],
    },

    domSync: {
      powerBarSelector: '.power-usage-box .power-bars .power-bar',
      currentInputUnit: 'A',
      vrTempBarSelector: '.power-bar[data-bar="vr-temp"]',
      fillSelector: '.power-bar__fill',
      labelSelector: '.power-bar__label',
      valueSelector: '.power-bar__value',
      lightThemeHints: ['default', 'corporate', 'light'],
      darkThemeHints: ['dark', 'cosmic'],
    },

    /**
     * Input current (A) bar rendering thresholds (visual-only).
     *
     * - < warnRel: normal grey fill
     * - >= warnRel and <= critRel: grey->yellow gradient
     * - > critRel: solid ASIC-temp-pill red with white text
     */
    inputCurrent: {
      /** If maxCurrentA is below this, use the "low" warn/crit ratios. */
      lowMaxAThreshold: 8,
      /** Warn ratio for devices below lowMaxAThreshold (e.g. <=8A). */
      lowWarnRel: 0.98,
      /** Crit ratio for devices below lowMaxAThreshold (e.g. <=8A). */
      lowCritRel: 0.99,
      warnRel: 0.96,
      critRel: 0.99,
    },

    /**
     * Input voltage (V) "normal" band.
     * Outside this band we show a grey->yellow gradient (visual-only).
     */
    inputVoltageBand: {
      low: 11.5,
      high: 12.6,
    },
    /** Voltage regulator temperature thresholds (°C). */
    vrTempBand: {
      warnC: 94,
      critC: 115,
    },

  },

  axisPadding: {
    hashrate: {
      windowPoints: 180,
      padPct: 0.06,
      padPctTop: 0.05,
      padPctBottom: 0.07,
      minPadThs: 0.03,
      flatPadPctOfMax: 0.005,
      maxPadPctOfMax: 0.25,
    },
    temp: {
      windowPoints: 180,
      padPct: 0.10,
      minPadC: 1.5,
      flatPadC: 2.0,
      maxPadC: 8.0,
    },
  },
  colors: {
    chartGridColor: '#80808040',
    textFallback: '#e5e7eb',
    hashrateBase: '#a564f6',
    vregTemp: '#2DA8B7',
    asicTemp: '#C84847',
    pillsText: '#ffffff',
    pillsDebugStroke: '#ff00ff',
  },
  xAxis: {
    // Keep X viewport stable and visually calm: always show a full hour.
    fixedWindowMs: 60 * 60 * 1000,
    // Zoom limits: 1h .. 3h, 15m step.
    minWindowMs: 60 * 60 * 1000,
    maxWindowMs: 3 * 60 * 60 * 1000,
    zoomStepMs: 15 * 60 * 1000,

    // Deterministic time-axis ticks / grid (e.g. 15-minute labels: :00, :15, :30, :45)
    tickStepMs: 15 * 60 * 1000,
  },
  yAxis: {
    hashrateMaxTicksDefault: 5,
    hashrateTickCountClamp: {
      // Previously hardcoded as 2..30 in setHashrateYAxisLabelCount.
      min: 2,
      max: 30,
    },
    minTickSteps: {
      hashrateMinStepThs: 0.005,
      tempMinStepC: 2,
    },
    hashrateSoftIncludeRel: 0.05,
  },

  graphGuard: {
    cfg: {
      confirmSamples: 2,
      // Live reference tolerance (pool sum) used to suppress short-lived history dips
      // that do not match the live reference.
      // Example: a dip from 6.35 TH/s to 5.82 TH/s is ~8.35% and should be rejected
      // when the live reference is stable.
      liveRefTolerance: 0.06,
      bigStepRel: 0.20,
      // Require live reference to be genuinely stable before gating history samples.
      liveRefStableSamples: 2,
      liveRefStableRel: 0.05,
    },
    thresholds: {
      // Previously hardcoded relThresholds in updateChartData/sanitizeLoadedHistory.
      hashrate1m: 0.01,
      hashrate10m: 0.02,
      hashrate1h: 0.08,
      hashrate1d: 0.10,
      vregTemp: 0.35,
      asicTemp: 0.35,
    },
    enableHashrateSpikeGuard: true,
  },

  historyDrain: {
    renderThrottleMs: 500,
    useThrottledRender: true,
    suppressChartUpdatesDuringDrain: false,
    chunkSize: 0,
  },

  smoothing: {
    hashrate1m: {
      enabled: true,
      fastIntervalMs: 6000,
      mediumIntervalMs: 12000,
      tensionFast: 0.45,
      tensionMedium: 0.18,
      tensionSlow: 0.12,
      cubicInterpolationMode: 'monotone',
      // Previously effectively 60 in applyHashrate1mSmoothing
      medianWindowPoints: 60,
      // Add a little smoothing as the user zooms out (15m steps).
      zoomBoostPerStep: 0.08,
      // EMA smoothing (time-based, converted to points)
      emaWindowMsMin: 0,
      emaWindowMsMax: 180000, // 3 min max smoothing at far zoom
      emaWindowMsPerStep: 20000, // +20s per zoom step
      emaMinPoints: 2,
      emaMaxPoints: 120,
      snapLastPoint: true,
    },
  },

  tempScale: {
    // Keep temp axis stable unless it needs to move by >= 1°C.
    hysteresisC: 1,
    // Adaptive temp axis padding (bottom / top).
    axisMinPadC: 1,
    axisMaxPadC: 2,
  },

  warmup: {
    // Keep boot sensor junk (0..9°C) from unlocking warmup.
    tempMinValidC: 10,
    // Absolute sanity cap for temps.
    tempMaxValidC: 130,
    // Sequencing delays (after first valid sample of each stage)
    vregDelayMs: 1337,
    asicDelayMs: 750,
    hash1mDelayMs: 250,
    // Hard-cut immediately on the strong signature (temps drop below min + no live hashrate).
    // The streak is only a fallback and should stay low to prevent any 0-line dip.
    restartDetectStreak: 1,
  },

  sanitize: {
    // Never plot 0°C either: in practice this is a boot/sensor artifact for these miners.
    // (Real operating temps are far above this; treating 0 as invalid prevents "spikes" to the 0-line,
    // including after page refresh when history is reloaded.)
    tempMinC: 0.1,
    tempMaxC: 130,
    // Hashrate is in H/s in the chart.
    // NOTE: A value of 0 during runtime is a boot/restart artifact in practice and should never be plotted.
    // Keep this tiny so legitimate low values (if any) still render.
    hashrateMinHs: 1,
  },

  startup: {
    bypassGuardSamples: 0,
    expectedUnlockRatio: 0.98,
    // Super smooth startup: apply stronger GraphGuard confirmation for 1m for a short window after restart.
    hr1mSmoothWindowMs: 15_000, // 15 seconds
    hr1mConfirmStartup: 5,
    hr1mConfirmNormal: 3,
    // Optional: reload the page after the smooth window finishes.
    // Guarded to trigger only once per miner restart (hard cut) and session-cooled down.
    hr1mReloadAfterSmooth: false,
    hr1mReloadCooldownMs: 600_000, // 10 minutes
  },
};
