import { clamp } from './math';

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
    // Temps cannot be negative; clamp min to 0 to avoid whitespace artifacts.
    const targetMin = Math.max(0, tm.mn - 2);
    const targetMax = tm.mx + 3;

    const maxTicks = Math.max(2, Math.round(Number(input.maxTicks || 7)));
    const rangeC = Math.max(0, targetMax - targetMin);
    const desired = rangeC / Math.max(1, maxTicks - 1);

    const niceStepsC = [0.5, 1, 2, 5, 10];
    let stepC = niceStepsC[niceStepsC.length - 1];
    for (const s of niceStepsC) {
      if (s >= desired) {
        stepC = s;
        break;
      }
    }
    if (stepC < input.tempMinStepC) stepC = input.tempMinStepC;

    let minAligned = Math.floor(targetMin / stepC) * stepC;
    const maxAligned = Math.ceil(targetMax / stepC) * stepC;

    minAligned = Math.max(0, minAligned);

    out.y_temp = {
      min: minAligned,
      max: maxAligned,
      stepSize: stepC,
      maxTicksLimit: maxTicks,
    };
  }

  return out;
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
