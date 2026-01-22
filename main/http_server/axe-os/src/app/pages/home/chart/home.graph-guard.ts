import { median } from './math';

export interface GraphGuardConfig {
  confirmSamples: number;
  liveRefTolerance: number;
  bigStepRel: number;
  liveRefStableSamples: number;
  liveRefStableRel: number;
  debug?: boolean;
  log?: (...args: any[]) => void;
}

type GuardState = {
  prev?: number;
  suspectDir?: -1 | 1;
  suspectCount: number;
  window: number[];
};

function isFiniteNumber(v: any): v is number {
  return typeof v === 'number' && Number.isFinite(v);
}

function log(cfg: GraphGuardConfig, ...args: any[]): void {
  if (!cfg.debug) return;
  (cfg.log ?? console.warn)(...args);
}

/**
 * GraphGuard smooths incoming samples while preventing extreme outliers and line breaks.
 * It is intentionally independent from Angular and Chart.js.
 */
export class GraphGuard {
  private cfg: GraphGuardConfig;
  private states = new Map<string, GuardState>();
  private liveRing: number[] = [];

  constructor(cfg: GraphGuardConfig) {
    this.cfg = { ...cfg };
  }

  configure(partial: Partial<GraphGuardConfig>): void {
    this.cfg = { ...this.cfg, ...partial };
  }

  reset(): void {
    this.states.clear();
    this.liveRing = [];
  }

  observeLiveRef(hs: number): void {
    if (!isFiniteNumber(hs) || hs <= 0) return;
    this.liveRing.push(hs);
    if (this.liveRing.length > 6) this.liveRing.shift();
  }

  isLiveRefStable(): boolean {
    const n = Math.max(1, Math.round(this.cfg.liveRefStableSamples));
    const rel = Math.max(0, Number(this.cfg.liveRefStableRel));
    if (this.liveRing.length < n) return false;

    let mn = Infinity;
    let mx = -Infinity;

    for (let i = this.liveRing.length - n; i < this.liveRing.length; i++) {
      const v = this.liveRing[i];
      if (!isFiniteNumber(v) || v <= 0) return false;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }

    const base = this.liveRing[this.liveRing.length - 1];
    if (!isFiniteNumber(base) || base <= 0) return false;
    return ((mx - mn) / base) <= rel;
  }

  apply(key: string, raw: any, relThreshold: number, liveRef?: number, confirmSamplesOverride?: number): number {
    const isHash = key.startsWith('hashrate_');
    const isTemp = key.toLowerCase().includes('temp');
    const minValid = (isHash || isTemp) ? 1 : -Infinity;
    const maxValid = isTemp ? 120 : Infinity;

    const current = Number(raw);
    const valid = Number.isFinite(current) && current > minValid && current < maxValid;

    const state: GuardState = this.states.get(key) ?? { suspectCount: 0, window: [] };
    const prev = state.prev;

    const fallback = state.window.length ? median(state.window) : (prev ?? 0);
    const candidate = valid ? current : fallback;

    // First sample: never invent a value.
    // If the first incoming point is invalid (NaN/0/boot junk), keep a gap until a valid
    // history sample arrives. This prevents "starts from 0" artifacts and keeps plotting
    // sourced from history only (no seeding from liveRef).
    if (prev === undefined) {
      if (!valid) {
        // Do not seed state.prev here; next valid sample will become the first point.
        this.states.set(key, state);
        return NaN;
      }

      const seed = candidate;
      state.prev = seed;
      state.window.push(seed);
      if (state.window.length > 9) state.window.shift();
      this.states.set(key, state);
      return seed;
    }

    // Live-gate: reject hashrate samples that are far from live pool sum.
    if (isHash && isFiniteNumber(liveRef) && liveRef > 0) {
      const live = liveRef;
      const liveStep = prev > 0 ? Math.abs(live - prev) / prev : 0;
      // Gate sensitivity:
      // Align the rejection threshold to the configured liveRefTolerance so short-lived
      // history dips that do NOT match the live pool sum are suppressed.
      // This is especially important shortly after restart where the 1m history stream
      // may emit a few transient low points even though the miner is already hashing.
      const baseGate = Math.max(0.03, Number(this.cfg.liveRefTolerance));
      // If liveRef itself is moving more than the gate, we're likely in a real change
      // (e.g. frequency change). In that case, don't block history samples.
      const effectiveGate = liveStep > baseGate ? 0.80 : baseGate;

      const liveRel = Math.abs(candidate - live) / live;
      if (liveRel > effectiveGate) {
        const relDiff = prev > 0 ? Math.abs(candidate - prev) / prev : Infinity;
        const bigStep = valid && prev > 0 && relDiff >= Number(this.cfg.bigStepRel);
        const liveStable = this.isLiveRefStable();

        if (liveStable && !bigStep) {
          log(this.cfg, '[GraphGuard:LiveGate]', key, { prev, candidate, live, liveRel, effectiveGate, liveStable, bigStep });
          state.prev = prev;
          state.window.push(prev);
          if (state.window.length > 9) state.window.shift();
          this.states.set(key, state);
          return prev;
        }

        log(this.cfg, '[GraphGuard:LiveGateBypass]', key, { prev, candidate, live, liveRel, effectiveGate, liveStable, bigStep });
        return prev;
      }
    }

    const diff = candidate - prev;
    const absDiff = Math.abs(diff);
    const relDiff = prev > 0 ? absDiff / prev : (absDiff > 0 ? Infinity : 0);
    const suspicious = valid && prev > 0 && relDiff > relThreshold;

    let out = candidate;
    let fastAccepted = false;

    if (suspicious) {
      if (isHash && isFiniteNumber(liveRef) && liveRef > 0) {
        const live = liveRef;
        const liveRel = Math.abs(candidate - live) / live;
        const bigStep = relDiff >= Number(this.cfg.bigStepRel);
        if (bigStep && liveRel <= Number(this.cfg.liveRefTolerance)) {
          out = candidate;
          fastAccepted = true;
          state.suspectCount = 0;
          state.suspectDir = undefined;
          log(this.cfg, '[GraphGuard:FastPath]', key, { prev, candidate, live, liveRel });
        }
      }

      if (!fastAccepted) {
        const dir: -1 | 1 = diff >= 0 ? 1 : -1;
        if (state.suspectDir === dir) state.suspectCount += 1;
        else {
          state.suspectDir = dir;
          state.suspectCount = 1;
        }

        const confirmN = Number.isFinite(confirmSamplesOverride as any)
          ? Number(confirmSamplesOverride)
          : Number(this.cfg.confirmSamples);

        if (state.suspectCount >= Math.max(1, Math.round(confirmN))) {
          out = candidate;
          state.suspectCount = 0;
          state.suspectDir = undefined;
        } else {
          out = prev;
        }
      }

      log(this.cfg, '[GraphGuard]', key, { prev, candidate, out, relDiff });
    } else {
      state.suspectCount = 0;
      state.suspectDir = undefined;
      if (!valid) out = prev;
    }

    state.prev = out;
    state.window.push(out);
    if (state.window.length > 9) state.window.shift();

    this.states.set(key, state);
    return out;
  }
}
