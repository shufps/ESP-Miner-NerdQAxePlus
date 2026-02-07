export type ChartZoomCfg = {
  minWindowMs: number;
  maxWindowMs: number;
  zoomStepMs: number;
};

export function normalizeZoomCfg(cfg: ChartZoomCfg): ChartZoomCfg {
  const minWindowMs = Math.max(1, Math.round(Number(cfg?.minWindowMs || 0)));
  const zoomStepMs = Math.max(1, Math.round(Number(cfg?.zoomStepMs || 0)));
  const maxWindowMs = Math.max(minWindowMs, Math.round(Number(cfg?.maxWindowMs || minWindowMs)));
  return { minWindowMs, maxWindowMs, zoomStepMs };
}

export function clampWindowMs(windowMs: number, cfg: ChartZoomCfg): number {
  const c = normalizeZoomCfg(cfg);
  const ms = Math.round(Number(windowMs || 0));
  return Math.min(c.maxWindowMs, Math.max(c.minWindowMs, ms));
}

export function stepWindowMs(currentMs: number, deltaMs: number, cfg: ChartZoomCfg): number {
  const c = normalizeZoomCfg(cfg);
  const next = Math.round(Number(currentMs || 0)) + Math.round(Number(deltaMs || 0));
  return clampWindowMs(next, c);
}

export function toggleWindowMs(currentMs: number, cfg: ChartZoomCfg): number {
  const c = normalizeZoomCfg(cfg);
  const mid = (c.minWindowMs + c.maxWindowMs) / 2;
  const cur = Math.round(Number(currentMs || 0));
  return cur <= mid ? c.maxWindowMs : c.minWindowMs;
}

export function zoomStepCount(windowMs: number, cfg: ChartZoomCfg): number {
  const c = normalizeZoomCfg(cfg);
  const clamped = clampWindowMs(windowMs, c);
  const steps = (clamped - c.minWindowMs) / c.zoomStepMs;
  return Math.max(0, Math.round(steps));
}

export function updateChartWithZoomAnimation(chart: any, durationMs: number = 160): void {
  if (!chart) return;
  const opts = chart.options || (chart.options = {});
  const prev = opts.animation;
  opts.animation = {
    duration: Math.max(0, Math.round(Number(durationMs || 0))),
    easing: 'easeOutQuad',
  };
  try {
    chart.update();
  } catch {
    try { chart.update?.('none'); } catch {}
  } finally {
    opts.animation = prev;
  }
}

export function formatZoomWindowLabel(windowMs: number, hourShort: string = 'h', minuteShort: string = 'm'): string {
  const totalMinutes = Math.max(0, Math.round(Number(windowMs || 0) / 60000));
  const hours = Math.floor(totalMinutes / 60);
  const minutes = totalMinutes % 60;

  if (hours > 0 && minutes > 0) {
    return `${hours}${hourShort} ${minutes}${minuteShort}`;
  }
  if (hours > 0) {
    return `${hours}${hourShort}`;
  }
  return `${minutes}${minuteShort}`;
}

export function shouldShowZoomWindowLabel(windowMs: number, cfg: ChartZoomCfg): boolean {
  const c = normalizeZoomCfg(cfg);
  return Number.isFinite(windowMs as any) ? windowMs >= (c.minWindowMs + c.zoomStepMs) : false;
}
