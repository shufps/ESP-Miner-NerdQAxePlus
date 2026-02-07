export function getHistoryOldestTimestampMs(history: any): number | null {
  if (!history) return null;
  const base = Number(history.timestampBase ?? 0);
  const timestamps: number[] = Array.isArray(history.timestamps) ? history.timestamps : [];
  if (!timestamps.length) return null;

  let minTs = Infinity;
  for (const rel of timestamps) {
    let tsAbs = Number(rel) + base;
    if (!Number.isFinite(tsAbs)) continue;
    // API safety: accept both seconds and milliseconds
    if (tsAbs > 0 && tsAbs < 1000000000000) tsAbs *= 1000;
    if (tsAbs < minTs) minTs = tsAbs;
  }

  return Number.isFinite(minTs) ? minTs : null;
}

export interface Hr1mStartFromHistoryInputs {
  hr1mStarted: boolean;
  startupUnlocked: boolean;
  histOk: boolean;
  histUnlockOk: boolean;
  isHistoryImporting: boolean;
}

/**
 * Decide if the 1m series is allowed to start from history.
 * - If startup is unlocked, require the usual histUnlockOk.
 * - If we're importing history (page load), allow a valid history sample to start.
 */
export function shouldStartHr1mFromHistory(input: Hr1mStartFromHistoryInputs): boolean {
  if (input.hr1mStarted) return true;
  if (input.startupUnlocked) return input.histUnlockOk;
  return input.isHistoryImporting && input.histOk;
}
