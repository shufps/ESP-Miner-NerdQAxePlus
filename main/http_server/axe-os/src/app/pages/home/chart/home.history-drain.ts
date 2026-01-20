import { Observable, Subscription, take } from 'rxjs';
import type { ISystemInfo } from '../../../models/ISystemInfo';

export interface HomeHistoryDrainerDeps {
  /** Fetches a chunk starting at `startTimestampMs` (milliseconds). */
  fetchInfo: (startTimestampMs: number, chunkSize: number) => Observable<ISystemInfo>;

  /** Imports a single history chunk into the chart state. */
  importHistoryChunk: (history: any) => void;

  /** Called when the drainer toggles running state. */
  setRunning?: (running: boolean) => void;

  /** Called when the drainer toggles suppression (component may skip render/persist while true). */
  setSuppressed?: (suppressed: boolean) => void;

  /** Called for throttled intermediate renders. */
  render?: () => void;

  /** Called once at the end of a successful or failed drain. */
  finalize?: () => void;

  /** Optional debug logger. */
  log?: (...args: any[]) => void;
}

export interface HomeHistoryDrainerOptions {
  chunkSize: number;
  renderThrottleMs: number;
  useThrottledRender: boolean;
}

export class HomeHistoryDrainer {
  private sub?: Subscription;
  private running = false;

  private renderTimer: any = null;
  private renderPending = false;
  private lastRenderMs = 0;

  private opts: HomeHistoryDrainerOptions;

  constructor(
    private deps: HomeHistoryDrainerDeps,
    options?: Partial<HomeHistoryDrainerOptions>
  ) {
    this.opts = {
      chunkSize: options?.chunkSize ?? 0,
      renderThrottleMs: options?.renderThrottleMs ?? 500,
      useThrottledRender: options?.useThrottledRender ?? true,
    };
  }

  public stop(): void {
    this.sub?.unsubscribe();
    this.sub = undefined;

    if (this.renderTimer != null) {
      clearTimeout(this.renderTimer);
      this.renderTimer = null;
    }

    this.renderPending = false;
    this.running = false;
    this.deps.setRunning?.(false);
    this.deps.setSuppressed?.(false);
  }

  /**
   * Imports the initial history chunk and, if it hasMore, drains the remaining chunks.
   */
  public ingest(initialHistory: any): void {
    this.deps.importHistoryChunk(initialHistory);
    this.requestRender();

    if (!initialHistory?.hasMore) {
      this.deps.setRunning?.(false);
      this.deps.setSuppressed?.(false);
      return;
    }

    // If we're already draining, don't start a second chain.
    if (this.running) {
      return;
    }

    const startNext = this.getLastAbsTimestampFromHistory(initialHistory);
    if (startNext == null) {
      this.finish(false);
      return;
    }

    this.running = true;
    this.deps.setRunning?.(true);
    this.deps.setSuppressed?.(true);

    this.fetchNext(startNext + 1);
  }

  private fetchNext(startTs: number): void {
    this.sub?.unsubscribe();
    this.sub = this.deps.fetchInfo(startTs, this.opts.chunkSize).pipe(take(1)).subscribe({
      next: (info) => {
        const h = (info as any)?.history;
        if (!h) {
          this.deps.log?.('[HistoryDrain] missing history payload, stopping');
          this.finish(true);
          return;
        }

        this.deps.importHistoryChunk(h);
        this.requestRender();

        if (h.hasMore) {
          const lastAbs = this.getLastAbsTimestampFromHistory(h);
          if (lastAbs == null) {
            this.finish(true);
            return;
          }
          this.fetchNext(lastAbs + 1);
        } else {
          this.finish(true);
        }
      },
      error: (err) => {
        this.deps.log?.('[HistoryDrain] failed', err);
        // Ensure the UI can continue updating after a drain error.
        this.finish(true);
      },
    });
  }

  private requestRender(): void {
    if (!this.opts.useThrottledRender) return;
    if (!this.deps.render) return;

    this.renderPending = true;

    const now = Date.now();
    const elapsed = now - this.lastRenderMs;
    const dueIn = Math.max(0, this.opts.renderThrottleMs - elapsed);

    if (this.renderTimer != null) {
      return;
    }

    this.renderTimer = setTimeout(() => {
      this.renderTimer = null;
      if (!this.renderPending) return;
      this.renderPending = false;
      this.lastRenderMs = Date.now();
      this.deps.render?.();
    }, dueIn);
  }

  private flushFinal(): void {
    if (this.renderTimer != null) {
      clearTimeout(this.renderTimer);
      this.renderTimer = null;
    }

    this.renderPending = false;
    this.lastRenderMs = Date.now();

    this.deps.finalize?.();
    this.deps.render?.();
  }

  private finish(finalFlush: boolean): void {
    this.running = false;
    this.deps.setRunning?.(false);
    this.deps.setSuppressed?.(false);

    if (finalFlush) {
      this.flushFinal();
    }
  }

  private getLastAbsTimestampFromHistory(history: any): number | null {
    if (!history?.timestamps?.length) return null;
    const maxRel = Math.max(...history.timestamps);
    let ts = Number(history.timestampBase ?? 0) + Number(maxRel ?? 0);
    if (ts > 0 && ts < 1_000_000_000_000) ts *= 1000;
    return Number.isFinite(ts) && ts > 0 ? ts : null;
  }
}
