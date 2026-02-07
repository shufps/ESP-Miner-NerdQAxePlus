import { clamp, median } from './math';
import type { ChartZoomCfg } from './home.chart-zoom';

export type Hashrate1mSmoothingCfg = {
  enabled: boolean;
  fastIntervalMs: number;
  mediumIntervalMs: number;
  tensionFast: number;
  tensionMedium: number;
  tensionSlow: number;
  cubicInterpolationMode: 'monotone' | 'default';
  medianWindowPoints: number;
  zoomBoostPerStep: number;
  emaWindowMsMin: number;
  emaWindowMsMax: number;
  emaWindowMsPerStep: number;
  emaMinPoints: number;
  emaMaxPoints: number;
  snapLastPoint: boolean;
};

export function applyHashrate1mSmoothing(
  dataset: any,
  labels: number[],
  cfg: Hashrate1mSmoothingCfg,
  windowMs: number,
  zoomCfg: ChartZoomCfg
): void {
  if (!dataset) return;
  if (!cfg || !cfg.enabled) {
    dataset.tension = 0;
    try { delete dataset.cubicInterpolationMode; } catch {}
    return;
  }

  const raw = Array.isArray(dataset.data) ? dataset.data : [];

  let medianIntervalMs = 0;
  if (labels && labels.length >= 3) {
    const diffs: number[] = [];
    const n = Math.min(cfg.medianWindowPoints, labels.length - 1);
    for (let i = labels.length - n; i < labels.length; i++) {
      const d = labels[i] - labels[i - 1];
      if (Number.isFinite(d) && d > 0) diffs.push(d);
    }
    medianIntervalMs = diffs.length ? median(diffs) : 0;
  }

  let tension = cfg.tensionSlow;
  if (medianIntervalMs && medianIntervalMs <= cfg.fastIntervalMs) tension = cfg.tensionFast;
  else if (medianIntervalMs && medianIntervalMs <= cfg.mediumIntervalMs) tension = cfg.tensionMedium;

  // Zoom-based extra smoothing (per step, clamped to tensionFast).
  const step = Math.max(0, Math.round(Number(cfg.zoomBoostPerStep || 0) * 1000) / 1000);
  if (step > 0) {
    const steps = Math.max(0, Math.round((windowMs - zoomCfg.minWindowMs) / zoomCfg.zoomStepMs));
    tension = Math.min(cfg.tensionFast, tension + steps * step);
  }

  dataset.tension = tension;
  dataset.cubicInterpolationMode = cfg.cubicInterpolationMode;

  // --- Data smoothing (EMA) to reduce spikes when zoomed out
  const steps = Math.max(0, Math.round((windowMs - zoomCfg.minWindowMs) / zoomCfg.zoomStepMs));
  const targetWindowMs = clamp(
    Number(cfg.emaWindowMsMin ?? 0) + steps * Number(cfg.emaWindowMsPerStep ?? 0),
    Number(cfg.emaWindowMsMin ?? 0),
    Number(cfg.emaWindowMsMax ?? 0)
  );

  let windowPoints = 0;
  if (medianIntervalMs > 0 && Number.isFinite(targetWindowMs) && targetWindowMs > 0) {
    windowPoints = Math.round(targetWindowMs / medianIntervalMs);
  }

  if (!Number.isFinite(windowPoints) || windowPoints < 2) {
    dataset.data = raw;
    return;
  }

  const minPoints = Math.max(2, Math.round(Number(cfg.emaMinPoints ?? 2)));
  const maxPoints = Math.max(minPoints, Math.round(Number(cfg.emaMaxPoints ?? minPoints)));
  const points = clamp(windowPoints, minPoints, maxPoints);

  if (!Number.isFinite(points) || points < 2) {
    dataset.data = raw;
    return;
  }

  const alpha = 2 / (points + 1);
  const smoothed: number[] = new Array(raw.length);
  let prev = Number.NaN;

  for (let i = 0; i < raw.length; i++) {
    const v = Number(raw[i]);
    if (!Number.isFinite(v)) {
      smoothed[i] = NaN;
      prev = NaN;
      continue;
    }
    prev = Number.isFinite(prev) ? (alpha * v + (1 - alpha) * prev) : v;
    smoothed[i] = prev;
  }

  if (cfg.snapLastPoint && raw.length) {
    const last = Number(raw[raw.length - 1]);
    if (Number.isFinite(last)) {
      smoothed[raw.length - 1] = last;
    }
  }

  dataset.data = smoothed;
}
