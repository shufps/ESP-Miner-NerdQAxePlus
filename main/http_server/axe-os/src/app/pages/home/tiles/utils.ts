// NOTE: These tile helpers are used by Angular templates; keep them framework-agnostic.
// Types are defined locally so utils.ts does not depend on home.cfg.ts type exports.

export type UptimeFormatCfg = {
  minutesInHour?: number;
  minutesInDay?: number;
  minutesInWeek?: number;
  minutesInMonth?: number;
  alwaysShowMinutes?: boolean;
  alwaysShowSeconds?: boolean;
};

export type PowerUsageAliasCfg = {
  milliAmpThreshold?: number;
  fallbackMaxA?: number;
  minRangeA?: number;
  minKeys?: string[];
  maxKeys?: string[];
};

export type BarDomSyncCfg = {
  powerBarSelector: string;
  fillSelector: string;
  labelSelector: string;
  valueSelector: string;
  currentInputUnit: string;
  vrTempBarSelector: string;
  lightThemeHints?: string[];
  darkThemeHints?: string[];
};

/**
 * Tile helper utilities.
 *
 * Keep these as pure functions so both HomeComponent and HomeExperimentalComponent
 * can expose them to templates without duplicating logic.
 */

export type ValueUnit = { value: string; unit: string };

/**
 * Centralized bar threshold defaults.
 *
 * Mirrors the old <app-gauge> behavior:
 *  - warn at 94% of the range
 *  - crit at 98% of the range
 */
export const BAR_THRESHOLDS = {
  warnRel: 0.94,
  // Critical earlier than the old default so red kicks in sooner (e.g. VR temp).
  critRel: 0.98,
} as const;

/**
 * ASIC temperature thresholds (used for ASIC °C + A1/A2/... chip squares).
 *
 * Requested:
 *  - warn at 80% of shutdown range
 *  - crit at 95% of shutdown range
 */
export const ASIC_TEMP_THRESHOLDS = {
  warnRel: 0.94,
  critRel: 0.98,
} as const;

export type BarLimits = {
  /** Inclusive lower bound for percentage conversion & thresholds */
  min: number;
  /** Inclusive upper bound for percentage conversion & max marker */
  max: number;
  /** Optional warn threshold as relative position within (min..max) */
  warnRel?: number;
  /** Optional crit threshold as relative position within (min..max) */
  critRel?: number;
};

/**
 * Central per-bar limits.
 *
 * Goal: templates should reference limits via constants instead of hardcoding.
 * Example: VR Temp is considered "normal" in the 20..120 °C range.
 */
export const BAR_LIMITS = {
  vrTemp: {
    min: 20,
    max: 120,
    warnRel: BAR_THRESHOLDS.warnRel,
    critRel: BAR_THRESHOLDS.critRel,
  },
} as const;

/**
 * Helper for bar gauges: convert a value/min/max to a clamped 0..100 percentage.
 */
export function toPct(
  value: number | null | undefined,
  min: number | null | undefined,
  max: number | null | undefined
): number {
  const v = Number(value);
  const mn = Number(min);
  const mx = Number(max);

  if (!isFinite(v) || !isFinite(mn) || !isFinite(mx) || mx <= mn) return 0;

  const clamped = Math.min(mx, Math.max(mn, v));
  return ((clamped - mn) / (mx - mn)) * 100;
}

/**
 * Highest ASIC temperature (prefers per-chip readings when available).
 * Spec: show the MAX ASIC temp, not the average.
 */
export function maxAsicTemp(info: any): number {
  const arr = info?.asicTemps;
  if (Array.isArray(arr)) {
    const vals = arr
      .map((v: any) => Number(v))
      .filter((v: number) => Number.isFinite(v) && v > 0);
    if (vals.length) return Math.max(...vals);
  }

  const t = Number(info?.temp);
  return Number.isFinite(t) ? t : 0;
}

/**
 * Split a human-readable value like "18.35 G" into numeric part + suffix unit.
 * Used to render units in a quieter style.
 */
export function splitHumanReadable(input: string | null | undefined): ValueUnit {
  const str = (input ?? '').toString().trim();
  if (!str) return { value: '', unit: '' };

  // Split on last whitespace (handles "18.35 G", "1,000 MHz", etc.)
  const lastSpace = str.lastIndexOf(' ');
  if (lastSpace === -1) return { value: str, unit: '' };

  const value = str.slice(0, lastSpace).trim();
  const unit = str.slice(lastSpace + 1).trim();
  return { value: value || str, unit };
}

/**
 * Bar threshold helpers for UI markers.
 *
 * Rules:
 *  - warn: value is >= 94% of the (min..max) range, but still below max
 *  - max:  value is >= max
 */
export function isBarWarn(
  value: number | null | undefined,
  min: number | null | undefined,
  max: number | null | undefined,
  warnRel: number = BAR_THRESHOLDS.warnRel
): boolean {
  const v = Number(value);
  const mn = Number(min);
  const mx = Number(max);
  if (!isFinite(v) || !isFinite(mn) || !isFinite(mx) || mx <= mn) return false;
  const warnAt = mn + (mx - mn) * warnRel;
  return v >= warnAt && v < mx;
}

/**
 * crit: value is >= 98% of the (min..max) range.
 *
 * Note: This intentionally includes `>= max` so templates can combine
 * `power-bar--crit` with `power-bar--max` when desired.
 */
export function isBarCrit(
  value: number | null | undefined,
  min: number | null | undefined,
  max: number | null | undefined,
  critRel: number = BAR_THRESHOLDS.critRel
): boolean {
  const v = Number(value);
  const mn = Number(min);
  const mx = Number(max);
  if (!isFinite(v) || !isFinite(mn) || !isFinite(mx) || mx <= mn) return false;
  const critAt = mn + (mx - mn) * critRel;
  return v >= critAt;
}

/**
 * ASIC temperature band helpers.
 * Same logic as isBarWarn/isBarCrit, but with ASIC-specific thresholds.
 */
export function isAsicTempWarn(
  value: number | null | undefined,
  min: number | null | undefined,
  max: number | null | undefined
): boolean {
  return isBarWarn(value, min, max, ASIC_TEMP_THRESHOLDS.warnRel);
}

export function isAsicTempCrit(
  value: number | null | undefined,
  min: number | null | undefined,
  max: number | null | undefined
): boolean {
  return isBarCrit(value, min, max, ASIC_TEMP_THRESHOLDS.critRel);
}

export function isBarMax(
  value: number | null | undefined,
  max: number | null | undefined
): boolean {
  const v = Number(value);
  const mx = Number(max);
  if (!isFinite(v) || !isFinite(mx)) return false;
  return v >= mx;
}

export function isBarOver(
  value: number | null | undefined,
  max: number | null | undefined
): boolean {
  const v = Number(value);
  const mx = Number(max);
  if (!isFinite(v) || !isFinite(mx)) return false;
  return v > mx;
}

/**
 * Safe ratio helper for threshold logic.
 * Returns 0 when inputs are not finite or max <= 0.
 */

/**
 * Returns true when a value is outside the inclusive [low, high] band.
 * Useful for "normal within band, warn when low/high" UI without additional states.
 */
export function isOutsideBand(value: any, low: number, high: number): boolean {
  const v = Number(value);
  const lo = Number(low);
  const hi = Number(high);
  if (!Number.isFinite(v) || !Number.isFinite(lo) || !Number.isFinite(hi)) return false;
  return v < lo || v > hi;
}


/** Returns true when value >= threshold. */
export function isAtLeast(value: any, threshold: number): boolean {
  const v = Number(value);
  const t = Number(threshold);
  if (!Number.isFinite(v) || !Number.isFinite(t)) return false;
  return v >= t;
}

/** Returns true when low <= value < high. */
export function isBetween(value: any, low: number, high: number): boolean {
  const v = Number(value);
  const lo = Number(low);
  const hi = Number(high);
  if (!Number.isFinite(v) || !Number.isFinite(lo) || !Number.isFinite(hi)) return false;
  return v >= lo && v < hi;
}


/**
 * Pool difficulty accessor that is tolerant to backend shape changes.
 * Some firmwares expose it as `diff`, others as `difficulty`.
 */
export function poolDiff(pool: unknown): number {
  const p: any = pool as any;
  const v = p?.diff ?? p?.difficulty ?? p?.poolDiff ?? p?.poolDifficulty;
  return Number.isFinite(v) ? Number(v) : 0;
}

/**
 * Abbreviate long identifiers like pool users / wallet strings.
 * Example: "bc1p...30ay" (first 4 + "..." + last 4).
 */
export function abbrevMiddle(
  input: string | null | undefined,
  head: number = 4,
  tail: number = 4
): string {
  const raw = (input ?? '').toString();
  if (!raw) return '';

  // If the identifier has an appended suffix separated by a dot (e.g. ".225_QX"),
  // hide that suffix completely and only shorten the main part.
  const core = raw.split('.')[0];
  if (!core) return '';

  const glue = ' ..... '; // space + 5 dots + space
  const minLen = head + tail + glue.length;
  if (core.length <= minLen) return core;
  return `${core.slice(0, head)}${glue}${core.slice(-tail)}`;
}

// -----------------------------------------------------------------------------
// Device-specific ASIC frequency scaling (Settings-aligned)
// -----------------------------------------------------------------------------

export type FreqBounds = { min: number; max: number };
export type VoltBounds = { min: number; max: number };

function normalizeVoltageToV(raw: any): number | null {
  const n = Number(raw);
  if (!Number.isFinite(n) || n <= 0) return null;
  // Settings coreVoltage is typically in mV (e.g. 1200). Measured is typically in V (e.g. 1.20).
  return n > 10 ? n / 1000 : n;
}

/**
 * Core-voltage bounds for the current device (Settings-aligned).
 *
 * IMPORTANT: Use the same source as the Settings screen:
 *   - `/asic` -> `voltageOptions` (array of mV values)
 *
 * We keep the bar MIN fixed at 0.9 V (requested) and scale the MAX dynamically
 * to the last available option (Settings/ASIC model). If options are missing,
 * we fall back and include current values so the bar stays usable.
 */
export function getAsicCoreVoltageBoundsFromAsic(
  info: any,
  asic: any,
  fallback: VoltBounds = { min: 0.9, max: 1.8 }
): VoltBounds {
  const raw = asic?.voltageOptions;

  const opts = Array.isArray(raw)
    ? raw
      .map(normalizeVoltageToV)
      .filter((v: any): v is number => typeof v === 'number' && Number.isFinite(v) && v > 0)
    : [];

  const curV = normalizeVoltageToV(info?.coreVoltageActual);
  const setV = normalizeVoltageToV(info?.coreVoltage);

  const min = fallback.min; // keep fixed

  // If Settings provides explicit voltageOptions (authoritative), use their max value as the bar MAX.
  // Do NOT extend the max with current readings, otherwise the MAX marker would never align to the
  // defined limit. Over-max readings are still handled by clamping the fill at 100%.
  let max = opts.length ? Math.max(...opts) : fallback.max;

  // If no options exist (older/unknown firmware), include current values so the bar stays usable.
  if (!opts.length) {
    if (setV != null) max = Math.max(max, setV);
    if (curV != null) max = Math.max(max, curV);
  }

  if (!Number.isFinite(max) || max <= min) {
    max = Math.max(fallback.max, min + 0.1);
    if (!opts.length) {
      if (setV != null) max = Math.max(max, setV);
      if (curV != null) max = Math.max(max, curV);
    }
  }

  return { min, max };
}

/**
 * Settings-aligned shutdown temperature (overheat) in °C.
 *
 * Some older firmwares expose `overheat_temp == 0` as "disabled".
 * Settings fixes this back to 70 °C - mirror the same behavior here.
 */
export function shutdownTempC(info: any, fallback: number = 70): number {
  let v = Number(info?.overheat_temp);
  if (!Number.isFinite(v) || v <= 0) v = fallback;

  // Avoid invalid ranges in the UI (min for the bar is 10 °C).
  v = Math.max(10, v);
  return v;
}

/**
 * Frequency bounds for the current device.
 *
 * IMPORTANT: Use the same source as the Settings screen:
 *   - `/asic` -> `frequencyOptions` (array of MHz values)
 *
 * We intentionally do NOT try to "guess" frequencies from random `/info` keys,
 * because Settings already provides the authoritative per-model table.
 *
 * If the endpoint/options are missing (older firmware), we fall back to the
 * legacy UI range 200..1000 MHz, while still including the current reading
 * so the bar never overflows.
 */
export function getAsicFrequencyBoundsFromAsic(
  info: any,
  asic: any
): FreqBounds {
  // Frequency bars should start at 0. The max should be device-specific.
  // We derive max from /asic frequencyOptions when available, otherwise fall back to the current reading.
  const raw = asic?.frequencyOptions;

  const nums = Array.isArray(raw)
    ? raw
        .map((v: any) => Number(v))
        .filter((v: number) => Number.isFinite(v) && v >= 0)
    : [];

  const cur = Number(info?.frequency);
  const curOk = Number.isFinite(cur) && cur >= 0;

  const candidates = curOk ? nums.concat([cur]) : nums;
  const max = candidates.length ? Math.max(...candidates) : (curOk ? cur : 1);

  // Safety net: keep max at least 1 (toPct would otherwise collapse).
  const safeMax = Number.isFinite(max) && max > 0 ? max : 1;

  return { min: 0, max: safeMax };
}

// -----------------------------------------------------------------------------
// Experimental Home dashboard helpers (bars + squares)
// -----------------------------------------------------------------------------

export type HomeTileDerivedFlags = {
  currentInputBarMaxWanted: boolean;
  vrTempBarCritWanted: boolean;
  isDualPool: boolean;
  hasChipTemps: boolean;
};

/**
 * Uptime formatting used in the Hashrate tile (months/weeks/days/hours/minutes).
 *
 * Previous behavior: no seconds (too noisy), always show minutes.
 */
export function formatUptime(totalSeconds: number, cfg: UptimeFormatCfg): string {
  const sec = Math.max(0, Math.floor(Number(totalSeconds ?? 0)));
  const totalMinutes = Math.floor(sec / 60);

  const minutesInHour = Number(cfg?.minutesInHour ?? 60);
  const minutesInDay = Number(cfg?.minutesInDay ?? 24 * minutesInHour);
  const minutesInWeek = Number(cfg?.minutesInWeek ?? 7 * minutesInDay);
  const minutesInMonth = Number(cfg?.minutesInMonth ?? 30 * minutesInDay);

  let rest = totalMinutes;

  const months = Math.floor(rest / minutesInMonth); rest %= minutesInMonth;
  const weeks = Math.floor(rest / minutesInWeek); rest %= minutesInWeek;
  const days = Math.floor(rest / minutesInDay); rest %= minutesInDay;
  const hours = Math.floor(rest / minutesInHour); rest %= minutesInHour;
  const mins = rest;

  const parts: string[] = [];
  if (months > 0) parts.push(`${months}mo`);
  if (weeks > 0) parts.push(`${weeks}w`);
  if (days > 0) parts.push(`${days}d`);
  if (hours > 0) parts.push(`${hours}h`);

  const alwaysMinutes = !!cfg?.alwaysShowMinutes;
  if (alwaysMinutes || mins > 0 || parts.length === 0) parts.push(`${mins}m`);

  // Optional: seconds (kept off by default in cfg)
  if (!!cfg?.alwaysShowSeconds && parts.length <= 1) {
    const sLeft = sec % 60;
    parts.push(`${sLeft}s`);
  }

  return parts.join(' ');
}

/**
 * Provide stable aliases for the Input Current power bar across firmware shapes.
 * Mutates `info` by adding: currentA, minCurrentA, maxCurrentA
 */
export function applyPowerUsageBarAliases(info: any, cfg: PowerUsageAliasCfg): void {
  if (!info) return;

  const milliAmpThreshold = Number(cfg?.milliAmpThreshold ?? 1000);
  const fallbackMaxA = Number(cfg?.fallbackMaxA ?? 6);
  const minRangeA = Number(cfg?.minRangeA ?? 6);

  const coerceA = (raw: any): number | undefined => {
    const n = Number(raw);
    if (!Number.isFinite(n)) return undefined;
    // Heuristic: values above ~1000 are very likely mA.
    return n > milliAmpThreshold ? n / 1000 : n;
  };

  const pickFirstA = (candidates: any[]): number | undefined => {
    for (const c of candidates) {
      const v = coerceA(c);
      if (v != null && Number.isFinite(v)) return v;
    }
    return undefined;
  };

  // current is already normalized to A in normalizeHomeTileInfo, but be defensive.
  const currentA = coerceA((info as any).currentA) ?? coerceA(info.current) ?? 0;

  const minKeys = Array.isArray(cfg?.minKeys) ? cfg.minKeys : [];
  const maxKeys = Array.isArray(cfg?.maxKeys) ? cfg.maxKeys : [];

  const minA = pickFirstA([
    ...minKeys.map((k) => (info as any)[k]),
  ]) ?? 0;

  let maxA = pickFirstA([
    ...maxKeys.map((k) => (info as any)[k]),
  ]);

  // Fallback: derive from maxPower/minVoltage (both already normalized above).
  if (!(maxA != null && Number.isFinite(maxA) && maxA > 0)) {
    const minV = Number(info.minVoltage);
    const maxP = Number(info.maxPower);
    const derivedMaxA = (Number.isFinite(minV) && minV > 0 && Number.isFinite(maxP))
      ? (maxP / minV)
      : NaN;
    if (Number.isFinite(derivedMaxA) && derivedMaxA > 0) maxA = derivedMaxA;
  }

  if (!(maxA != null && Number.isFinite(maxA) && maxA > 0)) maxA = fallbackMaxA;
  if (maxA <= minA) maxA = minA + minRangeA;

  (info as any).currentA = currentA;
  (info as any).minCurrentA = minA;
  (info as any).maxCurrentA = parseFloat(Number(maxA).toFixed(1));
}

export function computeCurrentInputBarMaxWanted(info: any): boolean {
  try {
    const v = Number((info as any)?.currentA);
    const mx = Number((info as any)?.maxCurrentA);
    return Number.isFinite(v) && Number.isFinite(mx) && mx > 0 && v >= mx;
  } catch {
    return false;
  }
}

export function computeVrTempBarCritWanted(info: any, limits: BarLimits = (BAR_LIMITS as any).vrTemp): boolean {
  try {
    return isBarCrit(Number((info as any)?.vrTemp), limits?.min, limits?.max, (limits as any)?.critRel);
  } catch {
    return false;
  }
}

/**
 * Normalize / derive all values that are used by bar + square tiles in the experimental Home.
 * Keeps HomeExperimentalComponent very thin: this function mutates `info` and returns derived flags.
 */
export function normalizeHomeTileInfo(
  info: any,
  args: {
    powerUsageAliases: PowerUsageAliasCfg;
    vrTempLimits?: BarLimits;
  }
): HomeTileDerivedFlags {
  if (!info) {
    return {
      currentInputBarMaxWanted: false,
      vrTempBarCritWanted: false,
      isDualPool: false,
      hasChipTemps: false,
    };
  }

  // --- Power usage / current / voltage normalization (UI wants neat rounding)
  try {
    info.minVoltage = parseFloat(Number(info.minVoltage).toFixed(1));
    info.maxVoltage = parseFloat(Number(info.maxVoltage).toFixed(1));
    info.minPower = parseFloat(Number(info.minPower).toFixed(1));
    info.maxPower = parseFloat(Number(info.maxPower).toFixed(1));
    info.power = parseFloat(Number(info.power).toFixed(1));
    // voltage/current come in mV/mA
    info.voltage = parseFloat((Number(info.voltage) / 1000).toFixed(1));
    info.current = parseFloat((Number(info.current) / 1000).toFixed(1));
  } catch {
    // ignore, keep raw
  }

  // --- Power usage bar aliases (currentA/minCurrentA/maxCurrentA)
  applyPowerUsageBarAliases(info, args.powerUsageAliases);

  const currentInputBarMaxWanted = computeCurrentInputBarMaxWanted(info);

  // --- Core voltage/temps come in mV / °C (already numeric)
  try {
    info.coreVoltageActual = parseFloat((Number(info.coreVoltageActual) / 1000).toFixed(2));
    info.coreVoltage = parseFloat((Number(info.coreVoltage) / 1000).toFixed(2));
    info.temp = parseFloat(Number(info.temp).toFixed(1));
    info.vrTemp = parseFloat(Number(info.vrTemp).toFixed(1));
    info.overheat_temp = parseFloat(Number(info.overheat_temp).toFixed(1));
  } catch {}

  const vrTempLimits = args.vrTempLimits ?? (BAR_LIMITS as any).vrTemp;
  const vrTempBarCritWanted = computeVrTempBarCritWanted(info, vrTempLimits);

  const isDualPool = Number((info as any)?.stratum?.activePoolMode ?? 0) === 1;

  const chipTemps = (info as any)?.asicTemps ?? [];
  const hasChipTemps =
    Array.isArray(chipTemps) &&
    chipTemps.length > 0 &&
    chipTemps.some((v: any) => v != null && !Number.isNaN(Number(v)) && Number(v) !== 0);

  return { currentInputBarMaxWanted, vrTempBarCritWanted, isDualPool, hasChipTemps };
}

// -----------------------------------------------------------------------------
// DOM sync helper (keeps special 100% fill colors without complicating templates)
// -----------------------------------------------------------------------------

export type RendererLike = {
  setStyle: (el: any, style: string, value: any) => void;
  removeStyle: (el: any, style: string) => void;
};

export type ElementRefLike<T extends HTMLElement = HTMLElement> = {
  nativeElement: T;
};

export class HomeBarDomSync {
  private currentInputBarFillEl?: HTMLElement;
  private currentInputBarTextEls?: HTMLElement[];
  private currentInputBarMaxApplied: boolean | null = null;

  private vrTempBarFillEl?: HTMLElement;
  private vrTempBarTextEls?: HTMLElement[];
  private vrTempBarCritApplied: boolean | null = null;

  constructor(
    private hostEl: ElementRefLike<HTMLElement>,
    private renderer: RendererLike,
    private cfg: BarDomSyncCfg
  ) {}

  /** Apply/remove the special 100% fill override for the Input Current bar. */
  public syncCurrentInputBarMaxFill(wanted: boolean, themeName: string): void {
    const w = !!wanted;

    // If nothing changed and we already resolved the element, bail fast.
    if (this.currentInputBarMaxApplied === w && this.currentInputBarFillEl) return;

    const fill = this.currentInputBarFillEl ?? this.findCurrentInputBarFillEl();
    if (!fill) return;
    this.currentInputBarFillEl = fill;

    if (this.currentInputBarMaxApplied === w) return;

    const bar = (fill.closest('.power-bar') as HTMLElement | null) ?? null;

    if (w) {
      this.renderer.setStyle(fill, 'background', 'var(--asic-temp-pill)');
      this.renderer.setStyle(fill, 'opacity', '1');

      // Light theme only: invert text color when the bar is fully filled/red.
      if (bar && this.isLightTheme(themeName)) {
        const label = bar.querySelector(this.cfg.labelSelector) as HTMLElement | null;
        const value = bar.querySelector(this.cfg.valueSelector) as HTMLElement | null;
        const els = [label, value].filter(Boolean) as HTMLElement[];
        this.currentInputBarTextEls = els;
        for (const el of els) this.renderer.setStyle(el, 'color', 'var(--text-control-color)');
      } else {
        this.clearCurrentInputBarTextOverride();
      }
    } else {
      this.renderer.removeStyle(fill, 'background');
      this.renderer.removeStyle(fill, 'opacity');
      this.clearCurrentInputBarTextOverride();
    }

    this.currentInputBarMaxApplied = w;
  }

  /** Apply/remove the special CRIT fill override for the VR temp bar. */
  public syncVrTempBarCritFill(wanted: boolean, themeName: string): void {
    const w = !!wanted;

    if (this.vrTempBarCritApplied === w && this.vrTempBarFillEl) return;

    const fill = this.vrTempBarFillEl ?? this.findVrTempBarFillEl();
    if (!fill) return;
    this.vrTempBarFillEl = fill;

    if (this.vrTempBarCritApplied === w) return;

    const bar = (fill.closest('.power-bar') as HTMLElement | null) ?? null;

    if (w) {
      this.renderer.setStyle(fill, 'background', 'var(--asic-temp-pill)');
      this.renderer.setStyle(fill, 'opacity', '1');

      if (bar && this.isLightTheme(themeName)) {
        const label = bar.querySelector(this.cfg.labelSelector) as HTMLElement | null;
        const value = bar.querySelector(this.cfg.valueSelector) as HTMLElement | null;
        const els = [label, value].filter(Boolean) as HTMLElement[];
        this.vrTempBarTextEls = els;
        for (const el of els) this.renderer.setStyle(el, 'color', 'var(--text-control-color)');
      } else {
        this.clearVrTempBarTextOverride();
      }
    } else {
      this.renderer.removeStyle(fill, 'background');
      this.renderer.removeStyle(fill, 'opacity');
      this.clearVrTempBarTextOverride();
    }

    this.vrTempBarCritApplied = w;
  }

  private clearCurrentInputBarTextOverride(): void {
    const els = this.currentInputBarTextEls;
    if (els && els.length) {
      for (const el of els) this.renderer.removeStyle(el, 'color');
    }
    this.currentInputBarTextEls = undefined;
  }

  private clearVrTempBarTextOverride(): void {
    const els = this.vrTempBarTextEls;
    if (els && els.length) {
      for (const el of els) this.renderer.removeStyle(el, 'color');
    }
    this.vrTempBarTextEls = undefined;
  }

  private findCurrentInputBarFillEl(): HTMLElement | null {
    const root = (this.hostEl as any)?.nativeElement as HTMLElement | null;
    if (!root) return null;

    const bars = Array.from(root.querySelectorAll(this.cfg.powerBarSelector)) as HTMLElement[];
    for (const bar of bars) {
      const unitEl = bar.querySelector('.power-bar__value .unit') as HTMLElement | null;
      const unit = (unitEl?.textContent ?? '').replace(/\s+/g, '').trim();
      if (unit === this.cfg.currentInputUnit) {
        const fill = bar.querySelector(this.cfg.fillSelector) as HTMLElement | null;
        if (fill) return fill;
      }
    }
    return null;
  }

  private findVrTempBarFillEl(): HTMLElement | null {
    const root = (this.hostEl as any)?.nativeElement as HTMLElement | null;
    if (!root) return null;

    const bar = root.querySelector(this.cfg.vrTempBarSelector) as HTMLElement | null;
    if (!bar) return null;
    return (bar.querySelector(this.cfg.fillSelector) as HTMLElement | null) ?? null;
  }

  private isLightTheme(themeName: string): boolean {
    const name = String(themeName || '').toLowerCase();

    const lightHints = Array.isArray(this.cfg.lightThemeHints) ? this.cfg.lightThemeHints : [];
    const darkHints = Array.isArray(this.cfg.darkThemeHints) ? this.cfg.darkThemeHints : [];

    if (darkHints.some((h) => name.includes(String(h).toLowerCase()))) return false;
    if (lightHints.some((h) => name.includes(String(h).toLowerCase()))) return true;

    // Fallback: infer from text color luminance (light theme -> dark text).
    try {
      const css = getComputedStyle(document.body);
      const c = (css.getPropertyValue('--text-basic-color') || css.color || '').trim();
      const rgb = this.parseCssColorToRgb(c);
      if (!rgb) return false;
      const lum = (0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b) / 255;
      return lum < 0.5;
    } catch {
      return false;
    }
  }

  private parseCssColorToRgb(input: string): { r: number; g: number; b: number } | null {
    const s = (input || '').trim();
    if (!s) return null;

    const m = s.match(/^rgba?\((\s*\d+\s*),\s*(\d+\s*),\s*(\d+\s*)(?:,\s*([\d.]+)\s*)?\)$/i);
    if (m) {
      const r = Number(m[1]);
      const g = Number(m[2]);
      const b = Number(m[3]);
      if ([r, g, b].every((v) => Number.isFinite(v))) {
        return { r: max(0, min(255, r)), g: max(0, min(255, g)), b: max(0, min(255, b)) };
      }
      return null;
    }

    const h = s.match(/^#([0-9a-f]{3}|[0-9a-f]{6})$/i);
    if (h) {
      const hex = h[1];
      if (hex.length == 3) {
        const r = int(hex[0] + hex[0], 16);
        const g = int(hex[1] + hex[1], 16);
        const b = int(hex[2] + hex[2], 16);
        return { r, g, b };
      }
      const r = int(hex.slice(0, 2), 16);
      const g = int(hex.slice(2, 4), 16);
      const b = int(hex.slice(4, 6), 16);
      return { r, g, b };
    }

    return null;

    function int(x: string, base: number): number {
      return parseInt(x, base);
    }
    function min(a: number, b: number): number {
      return a < b ? a : b;
    }
    function max(a: number, b: number): number {
      return a > b ? a : b;
    }
  }
}
