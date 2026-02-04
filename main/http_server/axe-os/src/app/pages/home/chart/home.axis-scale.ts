import { clamp } from './math';
import { HOME_CFG } from '../home.cfg';

export interface AxisPadCfg {
  hashrate: {
    windowPoints: number;
    padPct: number;
    padPctTop: number;
    padPctBottom: number;
    minPadThs: number;
    flatPadPctOfMax: number;
    maxPadPctOfMax: number;
  };
  temp: {
    windowPoints: number;
    padPct: number;
    minPadC: number;
    flatPadC: number;
    maxPadC: number;
  };
}

export interface AxisScaleInputs {
  labels: number[];
  hr1m: number[];
  hr10m?: number[] | null;
  hr1h?: number[] | null;
  hr1d?: number[] | null;
  vregTemp?: number[] | null;
  asicTemp?: number[] | null;

  xMinMs: number;
  xMaxMs: number;

  axisPadCfg: AxisPadCfg;
  maxTicks: number;
  hashrateMinStepThs: number;
  tempMinStepC: number;
  liveRefHs?: number;
}

export interface ComputedAxisBounds {
  y?: {
    min: number;
    max: number;
    stepSize?: number;
    maxTicksLimit?: number;
  };
  y_temp?: {
    min: number;
    max: number;
    stepSize?: number;
    maxTicksLimit?: number;
  };
}

export interface HashrateSoftIncludeInputs {
  labels: number[];
  xMinMs: number;
  xMaxMs: number;
  axisPadCfg: AxisPadCfg;
  maxTicks: number;
  hashrateMinStepThs: number;
  baseSeries: number[];
  otherSeries?: Array<number[] | null | undefined>;
  liveRefHs?: number;
  softIncludeRel?: number;
  /** Optionally reuse precomputed base bounds to avoid double work. */
  baseBounds?: ComputedAxisBounds['y'];
}

export interface HashrateSeriesSet {
  hr1m: number[];
  hr10m: number[];
  hr1h: number[];
  hr1d: number[];
}

export interface HashrateVisibility {
  hr1m: boolean;
  hr10m: boolean;
  hr1h: boolean;
  hr1d: boolean;
}

export interface HomeChartScaleInputs {
  labels: number[];
  xMinMs: number;
  xMaxMs: number;
  series: HashrateSeriesSet & { vregTemp: number[]; asicTemp: number[] };
  visibility: HashrateVisibility;
  axisPadCfg: AxisPadCfg;
  maxTicks: number;
  hashrateMinStepThs: number;
  tempMinStepC: number;
  liveRefHs?: number;
  softIncludeRel?: number;
  axisMinPadC?: number;
  axisMaxPadC?: number;
  tempHysteresisC?: number;
  prevTempMin?: number | null;
  prevTempMax?: number | null;
}
export interface TempBoundsInputs {
  labels: number[];
  vregTemp?: number[] | null;
  asicTemp?: number[] | null;
  xMinMs: number;
  xMaxMs: number;
  maxTicks: number;
  axisMinPadC?: number;
  axisMaxPadC?: number;
}
/**
 * Compute a width X window.
 *
 * Chart.js will auto-fit the x-range to existing data unless min/max are set.
 * This helper keeps the viewport stable (e.g. always 1 hour), even when only
 * a few points exist (or right after the history has been cleared).
 */
export function computeXWindow(
  labels: number[],
  windowMs: number,
  nowMs: number = Date.now(),
): { xMinMs: number; xMaxMs: number } {
  const safeWindow = Math.max(0, Math.round(Number(windowMs) || 0));

  const last = Array.isArray(labels) && labels.length ? Number(labels[labels.length - 1]) : NaN;
  const xMaxRaw = Number.isFinite(last) ? Math.max(nowMs, last) : nowMs;
  const xMaxMs = xMaxRaw;
  const xMinMs = xMaxMs - safeWindow;

  return { xMinMs, xMaxMs };
}

function isFiniteNumber(v: any): v is number {
  return typeof v === 'number' && Number.isFinite(v);
}

function minMax(vals: number[]): { mn?: number; mx?: number } {
  let mn = Infinity;
  let mx = -Infinity;
  for (const v of vals) {
    if (!isFiniteNumber(v)) continue;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  if (mn === Infinity || mx === -Infinity) return {};
  return { mn, mx };
}

function robustMinMax(vals: number[]): { mn?: number; mx?: number } {
  const finite = vals.filter(isFiniteNumber);
  if (!finite.length) return {};
  finite.sort((a, b) => a - b);
  const trim = Math.floor(finite.length * 0.02);
  const lo = finite[Math.min(finite.length - 1, trim)];
  const hi = finite[Math.max(0, finite.length - 1 - trim)];
  return { mn: lo, mx: hi };
}

function indicesForWindow(labels: number[], xMin: number, xMax: number): { from: number; to: number } {
  if (!Array.isArray(labels) || labels.length === 0) return { from: 0, to: 0 };
  let from = 0;
  while (from < labels.length && labels[from] < xMin) from++;
  let to = from;
  while (to < labels.length && labels[to] <= xMax) to++;
  return { from, to: Math.max(to, from) };
}

function collectWindowed(out: number[], arr: number[] | null | undefined, from: number, to: number): void {
  if (!arr || !Array.isArray(arr)) return;
  const end = Math.min(arr.length, to);
  for (let i = from; i < end; i++) {
    const v = arr[i];
    if (isFiniteNumber(v)) out.push(v);
  }
}

export function computeAxisBounds(input: AxisScaleInputs): ComputedAxisBounds {
  const { labels, xMinMs, xMaxMs } = input;
  const { from, to } = indicesForWindow(labels, xMinMs, xMaxMs);

  const hashVals: number[] = [];
  collectWindowed(hashVals, input.hr1m, from, to);
  collectWindowed(hashVals, input.hr10m, from, to);
  collectWindowed(hashVals, input.hr1h, from, to);
  collectWindowed(hashVals, input.hr1d, from, to);

  const hm = robustMinMax(hashVals);

  const tempVals: number[] = [];
  collectWindowed(tempVals, input.vregTemp, from, to);
  collectWindowed(tempVals, input.asicTemp, from, to);

  const tm = minMax(tempVals);

  const out: ComputedAxisBounds = {};

  // Hashrate axis
  if (hm.mn !== undefined && hm.mx !== undefined) {
    const cfgH = input.axisPadCfg.hashrate;
    const range = Math.max(1e6, hm.mx - hm.mn);

    const padPctFallback = Number(cfgH.padPct ?? 0.06);
    const padPctTop = Number(cfgH.padPctTop ?? padPctFallback);
    const padPctBottom = Number(cfgH.padPctBottom ?? padPctFallback);

    const minPadHs = Number(cfgH.minPadThs ?? 0.15) * 1e12;
    const flatPadPctOfMax = Number(cfgH.flatPadPctOfMax ?? 0.03);
    const maxPadPctOfMax = Number(cfgH.maxPadPctOfMax ?? 0.25);

    const maxAbs = Math.max(1, Math.abs(hm.mx));
    const flatPad = maxAbs * flatPadPctOfMax;

    const padTop = clamp(Math.max(range * padPctTop, minPadHs, flatPad), 0, maxAbs * maxPadPctOfMax);
    const padBottom = clamp(Math.max(range * padPctBottom, minPadHs, flatPad), 0, maxAbs * maxPadPctOfMax);

    // Never allow negative minima (hashrate cannot be negative).
    let adjMin = Math.max(0, hm.mn - padBottom);
    let adjMax = hm.mx + padTop;

    const live = input.liveRefHs;
    if (isFiniteNumber(live) && live > 0) {
      if (live > adjMax) adjMax = live + padTop;
      if (live < adjMin) adjMin = Math.max(0, live - padBottom);
    }

    const HS_PER_THS = 1e12;
    const rangeHs = Math.max(0, adjMax - adjMin);
    const rangeThs = rangeHs / HS_PER_THS;

    const maxTicks = Math.max(2, Math.round(Number(input.maxTicks || 7)));

    const niceStepsThs = [0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10];
    let stepThs = niceStepsThs[niceStepsThs.length - 1];

    let bestScore = Number.POSITIVE_INFINITY;
    for (const s of niceStepsThs) {
      if (s < input.hashrateMinStepThs) continue;
      const ticks = Math.floor(rangeThs / s) + 1;
      const score = Math.abs(ticks - maxTicks) + (ticks > maxTicks ? 0.25 : 0);
      if (score < bestScore || (score === bestScore && s > stepThs)) {
        bestScore = score;
        stepThs = s;
      }
    }
    if (stepThs < input.hashrateMinStepThs) stepThs = input.hashrateMinStepThs;

    const stepHs = stepThs * HS_PER_THS;
    let minAligned = Math.floor(adjMin / stepHs) * stepHs;
    const maxAligned = Math.ceil(adjMax / stepHs) * stepHs;

    // Clamp again after alignment (alignment can drop slightly below 0).
    minAligned = Math.max(0, minAligned);

    out.y = {
      min: minAligned,
      max: maxAligned,
      stepSize: stepHs,
      maxTicksLimit: maxTicks,
    };
  }

  // Temperature axis
  if (tm.mn !== undefined && tm.mx !== undefined) {
    // We intentionally keep temperature bounds *strict* so the pills match the
    // intended padding: bottom <= -2°C and top <= +3°C relative to data.
    //
    // Previously we aligned to a "nice" tick step (e.g. 5°C) via floor/ceil.
    // That can push the max well beyond +3°C (e.g. 61°C -> 65°C), which makes
    // the top padding look wrong.
    const padMin = Number(HOME_CFG.tempScale.axisMinPadC ?? 1);
    const padMax = Number(HOME_CFG.tempScale.axisMaxPadC ?? 2);
    const targetMin = Math.max(0, tm.mn - padMin);
    const targetMax = tm.mx + padMax;

    const maxTicks = Math.max(2, Math.round(Number(input.maxTicks || 7)));

    out.y_temp = {
      min: targetMin,
      max: targetMax,
      maxTicksLimit: maxTicks,
    };
  }

  return out;
}

export function computeTempBounds(input: TempBoundsInputs): ComputedAxisBounds['y_temp'] | undefined {
  const { labels, xMinMs, xMaxMs } = input;
  const { from, to } = indicesForWindow(labels, xMinMs, xMaxMs);

  const tempVals: number[] = [];
  collectWindowed(tempVals, input.vregTemp, from, to);
  collectWindowed(tempVals, input.asicTemp, from, to);

  const tm = minMax(tempVals);
  if (tm.mn === undefined || tm.mx === undefined) return undefined;

  const padMin = Number(input.axisMinPadC ?? 1);
  const padMax = Number(input.axisMaxPadC ?? 2);
  const targetMin = Math.max(0, tm.mn - padMin);
  const targetMax = tm.mx + padMax;

  const maxTicks = Math.max(2, Math.round(Number(input.maxTicks || 7)));

  return {
    min: targetMin,
    max: targetMax,
    maxTicksLimit: maxTicks,
  };
}

/**
 * Compute hashrate axis bounds based on a single base series and optionally
 * soft-include other series without rescaling the entire chart.
 */
export function computeHashrateBoundsSoftInclude(input: HashrateSoftIncludeInputs): ComputedAxisBounds['y'] | undefined {
  const { labels, xMinMs, xMaxMs } = input;
  const baseBounds = input.baseBounds ?? computeAxisBounds({
      labels,
      hr1m: input.baseSeries,
      hr10m: null,
      hr1h: null,
      hr1d: null,
      vregTemp: null,
      asicTemp: null,
      xMinMs,
      xMaxMs,
      axisPadCfg: input.axisPadCfg,
      maxTicks: input.maxTicks,
      hashrateMinStepThs: input.hashrateMinStepThs,
      tempMinStepC: 0,
      liveRefHs: input.liveRefHs,
    }).y;

  if (!baseBounds) return undefined;

  const softRel = Math.max(0, Number(input.softIncludeRel ?? 0));
  if (!softRel || !input.otherSeries?.length || !labels.length) return baseBounds;

  const range = Math.max(1, baseBounds.max - baseBounds.min);
  const maxExpand = range * softRel;

  const { from, to } = indicesForWindow(labels, xMinMs, xMaxMs);

  let otherMin = Infinity;
  let otherMax = -Infinity;
  for (const arr of input.otherSeries) {
    if (!arr || !arr.length) continue;
    const end = Math.min(arr.length, to);
    for (let i = from; i < end; i++) {
      const v = arr[i];
      if (!isFiniteNumber(v)) continue;
      if (v < otherMin) otherMin = v;
      if (v > otherMax) otherMax = v;
    }
  }

  let min = baseBounds.min;
  let max = baseBounds.max;

  if (otherMin !== Infinity) {
    min = Math.max(baseBounds.min - maxExpand, Math.min(baseBounds.min, otherMin));
  }
  if (otherMax !== -Infinity) {
    max = Math.min(baseBounds.max + maxExpand, Math.max(baseBounds.max, otherMax));
  }

  return { ...baseBounds, min, max };
}

/**
 * Select the base hashrate series for axis scaling.
 * Prefer 1m if visible, otherwise fall back to the first visible long-term series.
 */
export function selectBaseHashrateSeries(series: HashrateSeriesSet, visibility: HashrateVisibility): number[] {
  if (visibility.hr1m) return series.hr1m;
  if (visibility.hr10m) return series.hr10m;
  if (visibility.hr1h) return series.hr1h;
  if (visibility.hr1d) return series.hr1d;
  return series.hr1m;
}

/**
 * Build the list of other visible hashrate series (excluding the chosen base series).
 */
export function collectOtherHashrateSeries(
  series: HashrateSeriesSet,
  visibility: HashrateVisibility,
  base: number[]
): Array<number[] | null | undefined> {
  const out: Array<number[] | null | undefined> = [];
  if (visibility.hr1m && base !== series.hr1m) out.push(series.hr1m);
  if (visibility.hr10m && base !== series.hr10m) out.push(series.hr10m);
  if (visibility.hr1h && base !== series.hr1h) out.push(series.hr1h);
  if (visibility.hr1d && base !== series.hr1d) out.push(series.hr1d);
  return out;
}

export function computeHomeChartScales(input: HomeChartScaleInputs): {
  bounds: ComputedAxisBounds;
  tempAxisMin?: number;
  tempAxisMax?: number;
} {
  const {
    labels,
    xMinMs,
    xMaxMs,
    series,
    visibility,
    axisPadCfg,
    maxTicks,
    hashrateMinStepThs,
    tempMinStepC,
    liveRefHs,
    softIncludeRel,
    axisMinPadC,
    axisMaxPadC,
    tempHysteresisC,
    prevTempMin,
    prevTempMax,
  } = input;

  const baseHash = selectBaseHashrateSeries(series, visibility);

  const bounds: ComputedAxisBounds = computeAxisBounds({
    labels,
    hr1m: baseHash,
    hr10m: null,
    hr1h: null,
    hr1d: null,
    vregTemp: null,
    asicTemp: null,
    xMinMs,
    xMaxMs,
    axisPadCfg,
    maxTicks,
    hashrateMinStepThs,
    tempMinStepC,
    liveRefHs,
  });

  if (bounds.y && labels.length) {
    const otherSeries = collectOtherHashrateSeries(series, visibility, baseHash);
    const softY = computeHashrateBoundsSoftInclude({
      labels,
      xMinMs,
      xMaxMs,
      axisPadCfg,
      maxTicks,
      hashrateMinStepThs,
      baseSeries: baseHash,
      otherSeries,
      liveRefHs,
      softIncludeRel,
      baseBounds: bounds.y,
    });
    if (softY) bounds.y = softY;
  }

  bounds.y_temp = computeTempBounds({
    labels,
    vregTemp: series.vregTemp,
    asicTemp: series.asicTemp,
    xMinMs,
    xMaxMs,
    maxTicks,
    axisMinPadC,
    axisMaxPadC,
  });

  // Sticky temp axis with hysteresis:
  // - expand immediately when new data exceeds current bounds
  // - contract gradually when data relaxes, so the chart stays calm
  let tempAxisMin: number | undefined;
  let tempAxisMax: number | undefined;
  if (bounds.y_temp) {
    const hysteresis = Math.max(0, Number(tempHysteresisC ?? 0));
    let min = bounds.y_temp.min;
    let max = bounds.y_temp.max;

    if (Number.isFinite(prevTempMin as any) && Number.isFinite(prevTempMax as any)) {
      const prevMin = Number(prevTempMin);
      const prevMax = Number(prevTempMax);

      // Lower bound:
      // - move down immediately if needed (expand)
      // - move up at most by hysteresis per update (contract)
      if (min > prevMin + hysteresis) {
        min = Math.min(min, prevMin + hysteresis);
      } else {
        min = min < prevMin - hysteresis ? min : prevMin;
      }

      // Upper bound:
      // - move up immediately if needed (expand)
      // - move down at most by hysteresis per update (contract)
      if (max < prevMax - hysteresis) {
        max = Math.max(max, prevMax - hysteresis);
      } else {
        max = max > prevMax + hysteresis ? max : prevMax;
      }
    }

    bounds.y_temp.min = min;
    bounds.y_temp.max = max;
    tempAxisMin = min;
    tempAxisMax = max;
  }

  return { bounds, tempAxisMin, tempAxisMax };
}

export function applyAxisBoundsToChartOptions(chartOptions: any, bounds: ComputedAxisBounds): void {
  if (!chartOptions) return;
  const scales: any = chartOptions.scales || (chartOptions.scales = {});

  if (bounds.y) {
    scales.y = scales.y || {};
    scales.y.min = bounds.y.min;
    scales.y.max = bounds.y.max;
    scales.y.ticks = scales.y.ticks || {};
    if (isFiniteNumber(bounds.y.stepSize)) scales.y.ticks.stepSize = bounds.y.stepSize;
    if (isFiniteNumber(bounds.y.maxTicksLimit)) scales.y.ticks.maxTicksLimit = bounds.y.maxTicksLimit;
  } else {
    scales.y = scales.y || {};
    delete scales.y.min;
    delete scales.y.max;
  }

  if (bounds.y_temp) {
    scales.y_temp = scales.y_temp || {};
    scales.y_temp.min = bounds.y_temp.min;
    scales.y_temp.max = bounds.y_temp.max;
    scales.y_temp.ticks = scales.y_temp.ticks || {};
    if (isFiniteNumber(bounds.y_temp.stepSize)) scales.y_temp.ticks.stepSize = bounds.y_temp.stepSize;
    if (isFiniteNumber(bounds.y_temp.maxTicksLimit)) scales.y_temp.ticks.maxTicksLimit = bounds.y_temp.maxTicksLimit;
  } else {
    scales.y_temp = scales.y_temp || {};
    delete scales.y_temp.min;
    delete scales.y_temp.max;
  }
}
