import { Chart } from 'chart.js';
import { HOME_CFG } from '../home.cfg';
import { applyHomeDatasetRenderOrder, createHomeDatasets, HomeChartSeriesRefs } from './home.chart.datasets';

export interface HomeChartFactoryDeps {
  series: HomeChartSeriesRefs;
  translate: (key: string) => string;
  maxTicksLimit: number;
  getTimeFormatIs12h: () => boolean;
  formatHashrate: (v: number) => string;
  persistLegendVisibility: (visibility: boolean[]) => void;
  debugPillsLayout?: boolean;
}

export interface HomeChartConfig {
  chartData: any;
  chartOptions: any;
}

export function createHomeChartConfig(deps: HomeChartFactoryDeps): HomeChartConfig {
  const chartData = {
    labels: deps.series.labels,
    datasets: createHomeDatasets({ t: deps.translate, series: deps.series }),
  };

  applyHomeDatasetRenderOrder(chartData.datasets as any[]);

  const chartOptions: any = {
    animation: false,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        labels: {
          // color set by theme helper
          sort: (a: any, b: any) => a.datasetIndex - b.datasetIndex,
          filter: (legendItem: any, data: any) => {
            const ds = data?.datasets?.[legendItem?.datasetIndex];
            return !ds?.excludeFromLegend;
          },
        },
        onClick: (_evt: any, legendItem: any, legend: any) => {
          const chart = legend.chart;
          const index = legendItem.datasetIndex;
          const meta = chart.getDatasetMeta(index);

          // Toggle
          meta.hidden = meta.hidden === null ? !chart.data.datasets[index].hidden : null;
          chart.update();

          // Persist visibility
          const visibility = chart.data.datasets.map((_ds: any, i: number) => (chart.getDatasetMeta(i).hidden ? true : false));
          deps.persistLegendVisibility(visibility);
        },
      },
      tooltip: {
        callbacks: {
          title: (context: any) => {
            const date = new Date(context[0].parsed.x);
            const is12h = deps.getTimeFormatIs12h();
            return is12h
              ? date.toLocaleString('en-US', { hour: 'numeric', minute: '2-digit', hour12: true, month: 'short', day: 'numeric' })
              : date.toLocaleString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false, month: 'short', day: 'numeric' });
          },
          label: (x: any) => {
            if (x?.dataset?.yAxisID === 'y_temp') {
              return `${x.dataset.label}: ${Number(x.raw).toFixed(1)} °C`;
            }
            return `${x.dataset.label}: ${deps.formatHashrate(x.raw)}`;
          },
        },
      },
      valuePills: {
        enabled: true,
        paddingXPx: 7,
        paddingYPx: 6,
        hashratePillDatasetIndex: 0,
        debug: Boolean(deps.debugPillsLayout),
        defaultPlotGapPx: 8,
        defaultOuterPaddingPx: 2,
        minTotalGapPx: 10,
        minGapPx: 2,
      },
    },
    scales: {
      x: {
        type: 'time',
        time: {
          // 1h viewport is enforced elsewhere via scales.x.min/max.
          // Generate ticks every 15 minutes for a calmer, more informative axis.
          unit: 'minute',
          stepSize: Math.max(1, Math.round((Number(HOME_CFG.xAxis.tickStepMs) || 0) / 60000)),
          displayFormats: {
            minute: deps.getTimeFormatIs12h() ? 'h:mm A' : 'HH:mm',
            hour: deps.getTimeFormatIs12h() ? 'h:mm A' : 'HH:mm',
          },
        },
        // Chart.js sometimes chooses very dense minute ticks depending on adapter/locale.
        // Force a predictable tick set at 15-minute boundaries for the current viewport.
        afterBuildTicks: (scale: any) => {
          const STEP_MS = Math.max(1, Math.round(Number(HOME_CFG.xAxis.tickStepMs) || 0));
          const min = Number(scale?.min);
          const max = Number(scale?.max);
          if (!Number.isFinite(min) || !Number.isFinite(max) || STEP_MS <= 0) return;

          const start = Math.ceil(min / STEP_MS) * STEP_MS;
          const ticks: any[] = [];
          for (let t = start; t <= max; t += STEP_MS) {
            ticks.push({ value: t });
          }

          // Make sure we always have at least 2 ticks so the axis doesn't look "broken".
          if (ticks.length < 2) {
            ticks.length = 0;
            ticks.push({ value: min }, { value: max });
          }

          scale.ticks = ticks;
        },
        ticks: {
          // color set by theme helper
          autoSkip: false,
        },
        grid: {
          color: HOME_CFG.colors.chartGridColor,
          drawBorder: false,
          display: true,
        },
      },
      y: {
        ticks: {
          // color set by theme helper
          maxTicksLimit: deps.maxTicksLimit,
          autoSkip: false,
          includeBounds: true,
          callback: (value: number) => deps.formatHashrate(value),
        },
        grid: {
          display: false,
          drawBorder: false,
        },
      },
      y_temp: {
        position: 'right',
        ticks: {
          // color set by theme helper
          maxTicksLimit: deps.maxTicksLimit,
          callback: (value: number) => `${Math.round(Number(value))} °C`,
        },
        grid: {
          color: HOME_CFG.colors.chartGridColor,
          drawBorder: false,
        },
      },
    },
  };

  return { chartData, chartOptions };
}

export function createHomeChart(canvas: HTMLCanvasElement, chartData: any, chartOptions: any): Chart {
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    throw new Error('Home chart: 2d canvas context not available');
  }
  return new Chart(ctx, {
    type: 'line',
    data: chartData,
    options: chartOptions,
  });
}
