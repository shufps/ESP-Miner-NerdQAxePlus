import { HOME_CFG } from '../home.cfg';

// Theme helpers for Home chart.

export interface HomeChartTheme {
  textColor: string;
  textColorSecondary: string;
  gridColor: string;
}

export function readHomeChartTheme(): HomeChartTheme {
  const bodyStyle = getComputedStyle(document.body);
  const text = (bodyStyle.getPropertyValue('--card-text-color') || '').trim() || HOME_CFG.colors.textFallback;

  // Use centrally configured grid color.
  const gridColor = HOME_CFG.colors.chartGridColor;

  return {
    textColor: text,
    textColorSecondary: text,
    gridColor,
  };
}

export function applyHomeChartTheme(chartOptions: any, theme: HomeChartTheme = readHomeChartTheme()): void {
  if (!chartOptions) return;

  try {
    if (chartOptions?.plugins?.legend?.labels) {
      chartOptions.plugins.legend.labels.color = theme.textColor;
    }
  } catch {}

  try {
    const scales: any = chartOptions?.scales;
    if (scales?.x) {
      if (scales.x.ticks) scales.x.ticks.color = theme.textColorSecondary;
      if (scales.x.grid) scales.x.grid.color = theme.gridColor;
    }
    if (scales?.y) {
      if (scales.y.ticks) scales.y.ticks.color = theme.textColorSecondary;
    }
    if (scales?.y_temp) {
      if (scales.y_temp.ticks) scales.y_temp.ticks.color = theme.textColorSecondary;
      if (scales.y_temp.grid) scales.y_temp.grid.color = theme.gridColor;
    }
  } catch {}
}
