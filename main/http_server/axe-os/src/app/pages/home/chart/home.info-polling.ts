import {
  EMPTY,
  Observable,
  catchError,
  exhaustMap,
  interval,
  map,
  shareReplay,
  startWith,
  tap,
} from 'rxjs';

import type { ISystemInfo } from '../../../models/ISystemInfo';

export interface CreateSystemInfoPollingDeps {
  /** Poll interval in milliseconds */
  pollMs: number;

  /** Chunk size passed to the backend (0 = unlimited) */
  chunkSize: number;

  /** Fetch fresh system info starting at a given timestamp (ms). */
  fetchInfo: (startTimestampMs: number, chunkSize: number) => Observable<ISystemInfo>;

  /** Returns a safe default info object if a poll emits a falsy value. */
  defaultInfo: () => ISystemInfo;

  /** Returns the last persisted absolute timestamp in ms (or null if unknown). */
  getStoredLastTimestampMs: () => number | null;

  /** Optional: read a one-off forced start timestamp (ms). */
  getForceStartTimestampMs?: () => number | null;

  /** Optional: clear the forced start timestamp once consumed. */
  clearForceStartTimestampMs?: () => void;

  /** Optional logger (defaults to console.error). */
  logError?: (...args: any[]) => void;

  /** Optional side-effects for every successful info payload. */
  onInfo?: (info: ISystemInfo) => void;

  /** Optional mapping/normalization step (can include side-effects). */
  mapInfo?: (info: ISystemInfo) => ISystemInfo;
}

/**
 * Creates the polling pipeline used by HomeExperimentalComponent.
 *
 * - polls every `pollMs`
 * - computes a startTimestamp (lastTimestamp+1, capped to 1h window)
 * - fetches info via `fetchInfo` (errors are swallowed per tick)
 * - optional `onInfo` and `mapInfo` hooks
 * - shareReplay(1)
 */
export function createSystemInfoPolling$(deps: CreateSystemInfoPollingDeps): Observable<ISystemInfo> {
  const log = deps.logError ?? ((...args: any[]) => console.error(...args));

  return interval(deps.pollMs).pipe(
    startWith(0),
    exhaustMap(() => {
      let storedLastTimestamp = deps.getStoredLastTimestampMs();

      // Optional: forced start timestamp (debug)
      try {
        const forced = deps.getForceStartTimestampMs?.();
        if (Number.isFinite(forced as any) && (forced as number) > 0) {
          storedLastTimestamp = forced as number;
          deps.clearForceStartTimestampMs?.();
        }
      } catch {
        // ignore
      }

      const now = Date.now();
      const oneHourAgo = now - 3600 * 1000;
      // Cap the startTimestamp to be at most one hour ago
      const startTimestamp = storedLastTimestamp
        ? Math.max(storedLastTimestamp + 1, oneHourAgo)
        : oneHourAgo;

      return deps.fetchInfo(startTimestamp, deps.chunkSize).pipe(
        catchError((err) => {
          log('[HomeComponent] getInfo polling error', err);
          // Skip this tick; continue polling
          return EMPTY;
        })
      );
    }),
    tap((info) => {
      if (!info) return;
      if (!deps.onInfo) return;
      try {
        deps.onInfo(info);
      } catch (e) {
        log('[HomeComponent] onInfo callback error', e);
      }
    }),
    map((info) => {
      if (!info) return deps.defaultInfo();
      if (!deps.mapInfo) return info;
      try {
        return deps.mapInfo(info);
      } catch (e) {
        log('[HomeComponent] mapInfo callback error', e);
        return info;
      }
    }),
    shareReplay({ refCount: true, bufferSize: 1 })
  );
}
