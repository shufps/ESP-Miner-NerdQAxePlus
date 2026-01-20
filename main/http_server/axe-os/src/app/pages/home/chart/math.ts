/**
 * Shared tiny numeric/stat helpers (no Angular/Chart.js dependencies)
 *
 */

export function clamp(v: number, lo: number, hi: number): number {
  if (!Number.isFinite(v)) return lo;
  return Math.min(Math.max(v, lo), hi);
}

export function median(values: number[]): number {
  const arr = (values ?? []).filter(v => Number.isFinite(v)).slice().sort((a, b) => a - b);
  if (!arr.length) return 0;
  const mid = Math.floor(arr.length / 2);
  return (arr.length % 2) ? arr[mid] : (arr[mid - 1] + arr[mid]) / 2;
}

export function findLastFinite(data: any[]): number | null {
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
