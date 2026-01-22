export type WarmupStage =
  | 'READY'
  | 'LOCKED'
  | 'VREG_DELAY'
  | 'WAIT_ASIC'
  | 'ASIC_DELAY'
  | 'WAIT_HASH_LIVE'
  | 'HASH_DELAY'
  | 'WAIT_HASH_FLOW';

export interface HomeWarmupCfg {
  tempMinValidC: number;
  tempMaxValidC: number;
  vregDelayMs: number;
  asicDelayMs: number;
  hash1mDelayMs: number;
  restartDetectStreak: number;
}

export interface WarmupLiveInputs {
  nowMs: number;
  vregTempC?: number | null;
  asicTempC?: number | null;
  liveHashrateHs?: number | null;
  expectedHashrateHs?: number | null;
  /** True if we currently consider the system "alive" (used for restart detection). */
  systemOk?: boolean | null;
  /** True if live hashrate reached the startup unlock ratio vs expected (used for 1m warmup gate). */
  unlockOk?: boolean | null;
}

function isFiniteNumber(v: any): v is number {
  return typeof v === 'number' && Number.isFinite(v);
}

function isTempValidForWarmup(v: any, cfg: HomeWarmupCfg): boolean {
  if (!isFiniteNumber(v)) return false;
  return v >= cfg.tempMinValidC && v <= cfg.tempMaxValidC;
}

/**
 * Warmup/Restart state machine.
 *
 * Goal: after a miner restart, charts should not resume until sensors/hashrate are sane,
 * and then resume in a controlled order with small delays.
 */
export class HomeWarmupMachine {
  private cfg: HomeWarmupCfg;
  private stage: WarmupStage = 'READY';
  private stageSinceMs = 0;

  private bootLikeStreak = 0;
  private breakPending = false;

  // Series enable flags (used by component to gate data pushes)
  private vregEnabled = true;
  private asicEnabled = true;
  private hr1mEnabled = true;
  private otherHashEnabled = true;

  // Flow detector for 1m (we require at least one finite sample)
  private hr1mFlow = true;

  constructor(cfg: HomeWarmupCfg) {
    this.cfg = { ...cfg };
  }

  /** Hard reset (used when we detect a restart/boot situation). */
  reset(nowMs: number): void {
    this.stage = 'LOCKED';
    this.stageSinceMs = nowMs;
    this.bootLikeStreak = 0;
    this.breakPending = true;

    this.vregEnabled = false;
    this.asicEnabled = false;
    this.hr1mEnabled = false;
    this.otherHashEnabled = false;
    this.hr1mFlow = false;
  }

  /**
   * Observe live system values to advance warmup stages.
   * Call once per info-poll tick.
   */
  observeLive(input: WarmupLiveInputs): void {
    const nowMs = input.nowMs;

    const vregOk = isTempValidForWarmup(input.vregTempC, this.cfg);
    const asicOk = isTempValidForWarmup(input.asicTempC, this.cfg);
    const liveHs = isFiniteNumber(input.liveHashrateHs) ? (input.liveHashrateHs as number) : 0;
    const liveOk = isFiniteNumber(liveHs) && liveHs > 0;

    const expectedHs = isFiniteNumber(input.expectedHashrateHs) ? (input.expectedHashrateHs as number) : 0;
    const systemOk = input.systemOk !== false; // default true
    // Warmup uses an explicit unlock signal (computed by component from pill live vs expected).
    const unlockOk = input.unlockOk === true;

    // Restart detection should not rely on temperatures falling quickly.
    // We use "systemOk" and the startup-unlock signal to detect restart/boot even if temps remain high.
    // Frequency changes are safe because unlockOk stays true once live reaches the expected ratio.
    const bootLike = (!systemOk) || (!unlockOk && (!liveOk || !vregOk || !asicOk));
    if (bootLike) this.bootLikeStreak += 1;
    else this.bootLikeStreak = 0;

    if (this.stage === 'READY') {
      if (this.bootLikeStreak >= Math.max(1, Math.round(this.cfg.restartDetectStreak))) {
        this.reset(nowMs);
      }
      return;
    }

    // Warmup progression
    switch (this.stage) {
      case 'LOCKED': {
        if (vregOk) {
          this.stage = 'VREG_DELAY';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'VREG_DELAY': {
        if (nowMs - this.stageSinceMs >= Math.max(0, this.cfg.vregDelayMs)) {
          this.vregEnabled = true;
          this.stage = 'WAIT_ASIC';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'WAIT_ASIC': {
        if (asicOk) {
          this.stage = 'ASIC_DELAY';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'ASIC_DELAY': {
        if (nowMs - this.stageSinceMs >= Math.max(0, this.cfg.asicDelayMs)) {
          this.asicEnabled = true;
          this.stage = 'WAIT_HASH_LIVE';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'WAIT_HASH_LIVE': {
        // Start 1m only once the live hashrate is present AND the startup unlock condition is met
        // (computed from live pill vs expected). This enforces the "75% expected" requirement.
        if (liveOk && unlockOk) {
          this.stage = 'HASH_DELAY';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'HASH_DELAY': {
        if (nowMs - this.stageSinceMs >= Math.max(0, this.cfg.hash1mDelayMs)) {
          this.hr1mEnabled = true;
          this.stage = 'WAIT_HASH_FLOW';
          this.stageSinceMs = nowMs;
        }
        break;
      }

      case 'WAIT_HASH_FLOW': {
        // Flow is confirmed via notifyHr1mFlow().
        break;
      }
    }
  }

  /** Tell warmup that the 1m series actually received a finite sample. */
  notifyHr1mFlow(nowMs: number): void {
    if (this.hr1mFlow) return;
    this.hr1mFlow = true;

    if (this.stage === 'WAIT_HASH_FLOW') {
      this.otherHashEnabled = true;
      this.stage = 'READY';
      this.stageSinceMs = nowMs;
    }
  }

  /**
   * If true, the component should insert a single "break" sample (NaNs) once.
   * This cleanly ends curves before the restart gap.
   */
  consumeBreakPending(): boolean {
    if (!this.breakPending) return false;
    this.breakPending = false;
    return true;
  }

  getStage(): WarmupStage {
    return this.stage;
  }

  isVregEnabled(): boolean {
    return this.vregEnabled;
  }
  isAsicEnabled(): boolean {
    return this.asicEnabled;
  }
  isHr1mEnabled(): boolean {
    return this.hr1mEnabled;
  }
  isOtherHashEnabled(): boolean {
    return this.otherHashEnabled;
  }

  /** True if we are currently suppressing ALL updates (restart window). */
  isLocked(): boolean {
    return this.stage === 'LOCKED' || this.stage === 'VREG_DELAY';
  }
}
