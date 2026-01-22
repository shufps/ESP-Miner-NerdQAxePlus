import { Chart, Plugin } from 'chart.js';
import { TimeScale } from 'chart.js/auto';
import { HOME_CFG } from '../home.cfg';

type ValuePillsSide = 'left' | 'right';

export interface ValuePillsDatasetOverrides {
  plotGapPx?: number;
  outerPaddingPx?: number;
  bg?: string;
  fg?: string;
}

export interface ValuePillsPluginOptions {
  enabled?: boolean;
  // Hashrate pill: display value can be overridden (e.g. live pool sum)
  hashrateDisplayValue?: number;
  // Hashrate pill: which dataset index to anchor to (default: 0 / 1min)
  hashratePillDatasetIndex?: number;
  fontSizePx?: number;
  fontFamily?: string;
  paddingXPx?: number;
  paddingYPx?: number;
  borderRadiusPx?: number;
  minGapPx?: number;
  defaultPlotGapPx?: number;
  defaultOuterPaddingPx?: number;
  minTotalGapPx?: number;
  debug?: boolean;
}

interface ValuePill {
  datasetIndex: number;
  side: ValuePillsSide;
  axisId: string;
  text: string;
  value: number;
  targetY: number;
  // Optional Y-clamp (e.g. temp pills limited to ±3°C)
  clampMinY?: number;
  clampMaxY?: number;
  x: number;
  y: number;
  w: number;
  h: number;
  plotGapPx: number;
  outerPaddingPx: number;
  bg: string;
  fg: string;
  yUpLimit?: number;
  yDownLimit?: number;
}

function valuePillsMedian(values: number[]): number {
  const arr = values.filter(v => Number.isFinite(v)).slice().sort((a, b) => a - b);
  if (!arr.length) return 0;
  const mid = Math.floor(arr.length / 2);
  return arr.length % 2 ? arr[mid] : (arr[mid - 1] + arr[mid]) / 2;
}

function valuePillsFindLastFinite(data: any[]): number | null {
  for (let i = (data?.length ?? 0) - 1; i >= 0; i--) {
    const d = data[i];
    const v =
      typeof d === 'number'
        ? d
        : (d && typeof d === 'object' && typeof d.y === 'number' ? d.y : NaN);
    if (Number.isFinite(v)) return v;
  }
  return null;
}

function valuePillsApplyMinTotalGap(plotGapPx: number, outerPaddingPx: number, minTotalGapPx: number): { plotGapPx: number; outerPaddingPx: number } {
  if (plotGapPx + outerPaddingPx >= minTotalGapPx) return { plotGapPx, outerPaddingPx };
  return { plotGapPx: Math.max(plotGapPx, minTotalGapPx - outerPaddingPx), outerPaddingPx };
}

function valuePillsFormatValue(axisId: string, value: number, decimalsOverride?: number): string {
  // Pills: explicit decimals (Hash: 2, Temp: 1)
  if (axisId === 'y_temp') {
    const d = Number.isFinite(decimalsOverride) ? Number(decimalsOverride) : 1;
    return `${value.toFixed(d)} °C`;
  }
  // Hashrate pills: always display in TH/s (series values are in H/s)
  const d = Number.isFinite(decimalsOverride) ? Number(decimalsOverride) : 2;
  const v = Number(value);
  if (!Number.isFinite(v)) return `0.00 TH/s`;
  return `${(v / 1e12).toFixed(d)} TH/s`;
}

function valuePillsRoundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
  const radius = Math.max(0, Math.min(r, w / 2, h / 2));
  ctx.beginPath();
  ctx.moveTo(x + radius, y);
  ctx.arcTo(x + w, y, x + w, y + h, radius);
  ctx.arcTo(x + w, y + h, x, y + h, radius);
  ctx.arcTo(x, y + h, x, y, radius);
  ctx.arcTo(x, y, x + w, y, radius);
  ctx.closePath();
}

function valuePillsComputePills(chart: any, opts: ValuePillsPluginOptions, measureOnly: boolean): ValuePill[] {
  const o = opts ?? {};
  const enabled = o.enabled !== false;
  if (!enabled) return [];

  const fontSizePx = o.fontSizePx ?? 11;
  const fontFamily = o.fontFamily ?? 'system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif';
  const paddingXPx = o.paddingXPx ?? 8;
  const paddingYPx = o.paddingYPx ?? 4;
  const borderRadiusPx = o.borderRadiusPx ?? 10;
  const minTotalGapPx = o.minTotalGapPx ?? 12;

  const ctx: CanvasRenderingContext2D = chart.ctx;
  ctx.save();
  ctx.font = `bold ${fontSizePx}px ${fontFamily}`;

  const pills: ValuePill[] = [];

  chart.data.datasets.forEach((ds: any, datasetIndex: number) => {
    const axisId = ds.yAxisID || 'y';
    // Only show pills for Hashrate (1min) and temperatures
    if (!(axisId === 'y_temp' || datasetIndex === 0)) return;
    const side: ValuePillsSide = axisId === 'y_temp' ? 'right' : 'left';

    const overrides: ValuePillsDatasetOverrides = ds.pill || {};
    let plotGapPx = Number.isFinite(overrides.plotGapPx) ? Number(overrides.plotGapPx) : (o.defaultPlotGapPx ?? 6);
    let outerPaddingPx = Number.isFinite(overrides.outerPaddingPx) ? Number(overrides.outerPaddingPx) : (o.defaultOuterPaddingPx ?? 6);
    ({ plotGapPx, outerPaddingPx } = valuePillsApplyMinTotalGap(plotGapPx, outerPaddingPx, minTotalGapPx));

    const lastValue = valuePillsFindLastFinite(ds.data as any[]);
    if (lastValue === null) return;

    // Hashrate pill: text from live pool sum, Y-position from 1m chart history.
    const displayValue = (axisId === 'y' && Number.isFinite((o as any).hashrateDisplayValue))
      ? Number((o as any).hashrateDisplayValue)
      : lastValue;

    const text = valuePillsFormatValue(axisId, displayValue, axisId === 'y_temp' ? 1 : 2);
    const textW = ctx.measureText(text).width;
    const w = Math.ceil(textW + paddingXPx * 2);
    const h = Math.ceil(fontSizePx + paddingYPx * 2);

    pills.push({
      datasetIndex,
      side,
      axisId,
      text,
      value: lastValue,
      targetY: 0,
      x: 0,
      y: 0,
      w,
      h,
      plotGapPx,
      outerPaddingPx,
      bg: (overrides.bg || ds.borderColor || ds.backgroundColor || '#333') as string,
      fg: (overrides.fg || HOME_CFG.colors.pillsText) as string,
    });
  });

  ctx.restore();

  if (measureOnly) return pills;

  // Vertical anchoring: dataset-coupled (last valid point)
  pills.forEach(p => {
    const scale = chart.scales?.[p.axisId];
    if (!scale?.getPixelForValue) return;
    p.targetY = scale.getPixelForValue(p.value);
    p.y = p.targetY;

    // Temperature pills: allow only a small, scale-coupled vertical offset (+3°C / -2°C)
    if (p.axisId === 'y_temp') {
      const yUp = scale.getPixelForValue(p.value + 3);
      const yDown = scale.getPixelForValue(p.value - 2);
      p.yUpLimit = Math.min(yUp, yDown);
      p.yDownLimit = Math.max(yUp, yDown);
    }
  });

  return pills;
}

function valuePillsStackVertically(pills: ValuePill[], chartArea: any, opts: ValuePillsPluginOptions): void {
  if (!pills.length) return;

  const o = opts ?? {};
  const baseMinGap = o.minGapPx ?? 4;

  // sort by target y
  pills.sort((a, b) => a.targetY - b.targetY);

  const topBound = chartArea.top;
  const bottomBound = chartArea.bottom;

  const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

  // Allowed range per pill (keeps pills inside frame and, for temps, within a small scale-range)
  pills.forEach((p, i) => {
    const halfH = p.h / 2;
    const top = topBound + p.outerPaddingPx + halfH;
    const bot = bottomBound - p.outerPaddingPx - halfH;

    let minY = top;
    let maxY = bot;

    // Temperature pills: limit movement strictly to +3°C up and -2°C down (scale-coupled)
    if (p.axisId === 'y_temp' && Number.isFinite(p.yUpLimit) && Number.isFinite(p.yDownLimit)) {
      minY = Math.max(minY, p.yUpLimit!);
      maxY = Math.min(maxY, p.yDownLimit!);
    }

    // stash on object for later passes
    (p as any)._minY = minY;
    (p as any)._maxY = maxY;
    p.y = clamp(p.targetY, minY, maxY);
  });

  // Constraint relaxation (few passes) so we can push down and, if needed, pull up within ranges
  for (let iter = 0; iter < 6; iter++) {
    let changed = false;

    for (let i = 1; i < pills.length; i++) {
      const prev = pills[i - 1];
      const cur = pills[i];
      const minGap = Math.max(baseMinGap, Math.round((prev.h + cur.h) * 0.1)); // height-aware
      const minDist = prev.h / 2 + cur.h / 2 + minGap;
      const need = prev.y + minDist;

      if (cur.y < need) {
        const ny = clamp(need, (cur as any)._minY, (cur as any)._maxY);
        if (ny !== cur.y) {
          cur.y = ny;
          changed = true;
        }
      }
    }

    for (let i = pills.length - 2; i >= 0; i--) {
      const cur = pills[i];
      const next = pills[i + 1];
      const minGap = Math.max(baseMinGap, Math.round((next.h + cur.h) * 0.1)); // height-aware
      const minDist = next.h / 2 + cur.h / 2 + minGap;
      const need = next.y - minDist;

      if (cur.y > need) {
        const ny = clamp(need, (cur as any)._minY, (cur as any)._maxY);
        if (ny !== cur.y) {
          cur.y = ny;
          changed = true;
        }
      }
    }

    if (!changed) break;
  }

  // final clamp
  pills.forEach(p => {
    p.y = clamp(p.y, (p as any)._minY, (p as any)._maxY);
  });
}

function valuePillsEllipsizeToFit(ctx: CanvasRenderingContext2D, text: string, maxTextW: number): string {
  if (ctx.measureText(text).width <= maxTextW) return text;
  const ell = '…';
  let lo = 0;
  let hi = text.length;
  while (lo < hi) {
    const mid = Math.floor((lo + hi) / 2);
    const candidate = text.slice(0, mid) + ell;
    if (ctx.measureText(candidate).width <= maxTextW) lo = mid + 1;
    else hi = mid;
  }
  const cut = Math.max(0, lo - 1);
  return text.slice(0, cut) + ell;
}

function valuePillsDrawPills(chart: any, opts: ValuePillsPluginOptions, pills: ValuePill[]): void {
  const o = opts ?? {};
  const fontSizePx = o.fontSizePx ?? 11;
  const fontFamily = o.fontFamily ?? 'system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif';
  const paddingXPx = o.paddingXPx ?? 8;
  const paddingYPx = o.paddingYPx ?? 4;
  const borderRadiusPx = o.borderRadiusPx ?? 10;

  const ctx: CanvasRenderingContext2D = chart.ctx;
  const chartArea = chart.chartArea;
  if (!chartArea) return;
  const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

  // group per side and collision-resolve separately
  const left = pills.filter(p => p.side === 'left');
  const right = pills.filter(p => p.side === 'right');

  valuePillsStackVertically(left, chartArea, o);
  valuePillsStackVertically(right, chartArea, o);

  ctx.save();
  ctx.font = `bold ${fontSizePx}px ${fontFamily}`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';

  const drawOne = (p: ValuePill) => {
    const minTotalGapPx = o.minTotalGapPx ?? 12;
    const gap = Math.max(p.plotGapPx + p.outerPaddingPx, minTotalGapPx);
    const outer = p.outerPaddingPx;

    // Reserve a strict band outside the plot area so pills never drift into the chart.
    const bandMin = (p.side === 'left') ? outer : (chartArea.right + gap);
    const bandMax = (p.side === 'left') ? (chartArea.left - gap) : (chart.width - outer);
    const bandW = Math.max(0, bandMax - bandMin);

    // If the pill is wider than the available band, ellipsize to fit the band.
    if (p.w > bandW) {
      const maxTextW = Math.max(0, bandW - paddingXPx * 2);
      p.text = valuePillsEllipsizeToFit(ctx, p.text, maxTextW);
      p.w = Math.ceil(ctx.measureText(p.text).width + paddingXPx * 2);
    }

    const scale = chart.scales?.[p.axisId];
    const fallbackCenter = (p.side === 'left') ? (bandMax - p.w / 2) : (bandMin + p.w / 2);
    const center = scale ? (scale.left + scale.right) / 2 : fallbackCenter;

    let x = clamp(center - p.w / 2, bandMin, bandMax - p.w);

    p.x = x;

    // draw pill
    ctx.fillStyle = p.bg;
    valuePillsRoundRect(ctx, p.x, p.y - p.h / 2, p.w, p.h, borderRadiusPx);
    ctx.fill();

    ctx.fillStyle = p.fg;
    ctx.fillText(p.text, p.x + p.w / 2, p.y);
  };

  left.forEach(drawOne);
  right.forEach(drawOne);

  if (o.debug) {
    ctx.save();
    ctx.strokeStyle = HOME_CFG.colors.pillsDebugStroke;
    ctx.lineWidth = 1;
    ctx.strokeRect(chartArea.left, chartArea.top, chartArea.right - chartArea.left, chartArea.bottom - chartArea.top);
    ctx.restore();
  }

  ctx.restore();
}

export const valuePillsPlugin: Plugin = {
  id: 'valuePills',
  beforeLayout: (chart: any) => {
    const opts = chart.options?.plugins?.valuePills as ValuePillsPluginOptions | undefined;
    if (!opts?.enabled) return;

    // Compute pill sizes early, then reserve scale width via afterFit wrapper (no layout padding bloat).
    const pills = valuePillsComputePills(chart, opts, true);

    const reqByAxis: Record<string, number> = {};
    pills.forEach(p => {
      const minTotalGapPx = (opts.minTotalGapPx ?? 12);
      const gap = Math.max((p.plotGapPx ?? 0) + (p.outerPaddingPx ?? 0), minTotalGapPx);
      // Ensure the scale is wide enough so the full pill text fits (no ellipsis/clipping).
      const req = Math.ceil(p.w + gap + p.outerPaddingPx);
      reqByAxis[p.axisId] = Math.max(reqByAxis[p.axisId] ?? 0, req);
    });
    (chart as any)._valuePillsReqScaleWidth = reqByAxis;

    // Wrap afterFit once per scale to enforce minimum width.
    Object.keys(chart.scales ?? {}).forEach((axisId) => {
      const scale: any = chart.scales[axisId];
      if (!scale || scale._valuePillsAfterFitWrapped) return;

      const original = scale.afterFit?.bind(scale);
      scale.afterFit = () => {
        if (original) original();
        const req = (chart as any)._valuePillsReqScaleWidth?.[axisId];
        if (Number.isFinite(req)) {
          scale.width = Math.max(scale.width, Number(req));
        }
      };
      scale._valuePillsAfterFitWrapped = true;
    });
  },
  afterDatasetsDraw: (chart: any) => {
    const opts = chart.options?.plugins?.valuePills as ValuePillsPluginOptions | undefined;
    if (!opts?.enabled) return;

    const pills = valuePillsComputePills(chart, opts, false);
    const ready = pills.filter(p => Number.isFinite(p.y));
    valuePillsDrawPills(chart, opts, ready);
  },
};

let _registered = false;

export function registerValuePillsPlugin(): void {
  if (_registered) return;
  _registered = true;
  // TimeScale is required for time-based x-axis in Chart.js
  Chart.register(TimeScale, valuePillsPlugin);
}
