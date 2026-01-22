import { HOME_CFG } from '../home.cfg';

// Auto-extracted from the original HomeExperimentalComponent constructor.
// Visual/design layer only.

export interface HomeChartSeriesRefs {
  labels: number[];
  hr1m: number[];
  hr10m: number[];
  hr1h: number[];
  hr1d: number[];
  vregTemp: number[];
  asicTemp: number[];
}

export function createHomeDatasets(opts: { t: (key: string) => string; series: HomeChartSeriesRefs }): any[] {
  const { t, series } = opts;
  const HR_BASE_COLOR = HOME_CFG.colors.hashrateBase;

  function hrColor(alpha: number = 1): string {
    if (alpha >= 1) return HR_BASE_COLOR;
    const hex = HR_BASE_COLOR.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  }

  function hr1mAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;
    if (!ctx || !chartArea) return hrColor(0.3);
    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, hrColor(0.3));
    gradient.addColorStop(1, hrColor(0));
    return gradient;
  }

  function vrTempAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;

    const baseHex = HOME_CFG.colors.vregTemp;
    const topAlpha = 0.01;
    const bottomAlpha = 0.12;

    const hex = baseHex.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    const rgba = (alpha: number) => `rgba(${r}, ${g}, ${b}, ${alpha})`;

    if (!ctx || !chartArea) return rgba(topAlpha);

    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, rgba(topAlpha));
    gradient.addColorStop(1, rgba(bottomAlpha));
    return gradient;
  }

  function asicTempAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;

    const baseHex = HOME_CFG.colors.asicTemp;
    const topAlpha = 0.01;
    const bottomAlpha = 0.21;

    const hex = baseHex.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    const rgba = (alpha: number) => `rgba(${r}, ${g}, ${b}, ${alpha})`;

    if (!ctx || !chartArea) return rgba(topAlpha);

    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, rgba(topAlpha));
    gradient.addColorStop(1, rgba(bottomAlpha));
    return gradient;
  }

  return [

  {
    type: 'line',
    label: t('HOME.HASHRATE_1M'),
    data: series.hr1m,
    yAxisID: 'y',
    fill: 'start',
    backgroundColor: (context: any) => hr1mAreaGradient(context),
    borderColor: hrColor(1),
    pill: { bg: hrColor(1) },
    tension: 0.6,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 2.1
  },
  {
    type: 'line',
    label: t('HOME.HASHRATE_10M'),
    data: series.hr10m,
    yAxisID: 'y',
    fill: false,
    backgroundColor: hrColor(0),
    borderColor: hrColor(0.9),
    tension: .4,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 1.8,
    borderDash: [1, 4],
    borderCapStyle: 'round'
  },
  {
    type: 'line',
    label: t('HOME.HASHRATE_1H'),
    data: series.hr1h,
    yAxisID: 'y',
    fill: false,
    backgroundColor: hrColor(0),
    borderColor: hrColor(0.8),
    tension: .4,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 1.8,
    borderDash: [8, 2]
  },
  {
    type: 'line',
    label: t('HOME.HASHRATE_1D'),
    data: series.hr1d,
    hidden: true,
    excludeFromLegend: true,
    yAxisID: 'y',
    fill: false,
    backgroundColor: hrColor(0),
    borderColor: hrColor(0.8),
    tension: .4,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 1.8,
    borderDash: [14, 8]
  },
  {
    type: 'line',
    label: t('PERFORMANCE.VR_TEMP_LEGEND'),
    data: series.vregTemp,
    yAxisID: 'y_temp',
    fill: true,
    borderColor: HOME_CFG.colors.vregTemp,
    backgroundColor: (context: any) => vrTempAreaGradient(context),
    tension: .4,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 1.4
  },
  {
    type: 'line',
    label: t('PERFORMANCE.ASIC_TEMP_LEGEND'),
    data: series.asicTemp,
    yAxisID: 'y_temp',
    fill: true,
    borderColor: HOME_CFG.colors.asicTemp,
    backgroundColor: (context: any) => asicTempAreaGradient(context),
    tension: .4,
    cubicInterpolationMode: 'monotone',
    pointRadius: 0,
    borderWidth: 1.3
  }
  ];
}

export function applyHomeDatasetRenderOrder(datasets: any[]): void {
  const hr1m: any = datasets?.[0];
  const hr10m: any = datasets?.[1];
  const hr1h: any = datasets?.[2];
  const hr1d: any = datasets?.[3];

  const vr: any = datasets?.[4];
  const asic: any = datasets?.[5];

  if (hr1m) hr1m.order = 0;
  if (asic) asic.order = 1;
  if (vr) vr.order = 2;

  if (hr10m) hr10m.order = 10;
  if (hr1h) hr1h.order = 11;
  if (hr1d) hr1d.order = 12;
}
