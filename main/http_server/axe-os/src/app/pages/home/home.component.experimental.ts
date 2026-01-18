import { Component, AfterViewChecked, OnInit, OnDestroy } from '@angular/core';
import { interval, map, Observable, shareReplay, startWith, switchMap, tap, Subscription, take, exhaustMap, catchError, EMPTY } from 'rxjs';
import { HashSuffixPipe } from '../../pipes/hash-suffix.pipe';
import { SystemService } from '../../services/system.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { Chart, Plugin } from 'chart.js';  // Import Chart.js
import { ElementRef, ViewChild } from "@angular/core";
import { TimeScale } from "chart.js/auto";
import { NbThemeService } from '@nebular/theme';
import { NbTrigger } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { LocalStorageService } from '../../services/local-storage.service';
import { IPool } from 'src/app/models/IStratum';
import {
  getPoolIconUrl as resolvePoolIconUrl,
  getQuickLink,
  supportsPing,
  isLocalHost,
  DEFAULT_POOL_ICON_URL,
  DEFAULT_EXTERNAL_POOL_ICON_URL,
} from './home.quicklinks';
@Component({
  selector: 'app-home-experimental',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})

export class HomeExperimentalComponent implements AfterViewChecked, OnInit, OnDestroy {
  @ViewChild('myChart') ctx!: ElementRef<HTMLCanvasElement>;

  private HR_BASE_COLOR: string = '#a564f6';

  // Build the hashrate graph color from the base hex color and a given alpha (opacity).
  private hrColor(alpha: number = 1): string {
    if (alpha >= 1) return this.HR_BASE_COLOR;
    const hex = this.HR_BASE_COLOR.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  }

  // Build the 1-minute hashrate area background as a classic vertical gradient (80% -> 0%).
  private hr1mAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;

    // Chart.js can call backgroundColor before layout; fall back until chartArea exists.
    if (!ctx || !chartArea) return this.hrColor(0.3);

    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, this.hrColor(0.3));
    gradient.addColorStop(1, this.hrColor(0));
    return gradient;
  }

  // Build the VR temperature area background as a reversed classic vertical gradient (25% -> 70%).
  private vrTempAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;

    // Settings for gradient.
    const baseHex = '#2DA8B7';
    const topAlpha = 0.01;
    const bottomAlpha = 0.12;

    const hex = baseHex.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    const rgba = (alpha: number) => `rgba(${r}, ${g}, ${b}, ${alpha})`;

    // Chart.js can call backgroundColor before layout; fall back until chartArea exists.
    if (!ctx || !chartArea) return rgba(topAlpha);

    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, rgba(topAlpha));
    gradient.addColorStop(1, rgba(bottomAlpha));
    return gradient;
  }

  // Build the ASIC temperature area background as a reversed classic vertical gradient (25% -> 70%).
  private asicTempAreaGradient(context: any): CanvasGradient | string {
    const chart = context?.chart;
    const ctx = chart?.ctx;
    const chartArea = chart?.chartArea;

    // Settings for gradient.
    const baseHex = '#C84847';
    const topAlpha = 0.01;
    const bottomAlpha = 0.21;

    const hex = baseHex.replace('#', '');
    const r = parseInt(hex.substring(0, 2), 16);
    const g = parseInt(hex.substring(2, 4), 16);
    const b = parseInt(hex.substring(4, 6), 16);
    const rgba = (alpha: number) => `rgba(${r}, ${g}, ${b}, ${alpha})`;

    // Chart.js can call backgroundColor before layout; fall back until chartArea exists.
    if (!ctx || !chartArea) return rgba(topAlpha);

    const gradient = ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
    gradient.addColorStop(0, rgba(topAlpha));
    gradient.addColorStop(1, rgba(bottomAlpha));
    return gradient;
  }

  // --- GraphGuard
  // Step-Confirmation: how many consecutive "suspicious" samples in the same direction
  // are required before accepting a step. Increase to 3 to be more conservative.
  private graphGuardConfirmSamples: number = 2;
  // If a "suspicious" hashrate step matches the live pool-sum reference within this tolerance,
  // accept immediately (so the Y-scale reacts in 1–2 ticks).
  private graphGuardLiveRefTolerance: number = 0.15;
  // A step >= this relative delta vs previous sample is treated as a likely real change (e.g. freq up/down)
  // and will not be blocked by the live-ref gate. (5s updates -> reacts in ~10s with confirmSamples=2)
  private graphGuardBigStepRel: number = 0.20;
  // Live pool-sum stability detector to avoid trusting a single live tick.
  private graphGuardLiveRefStableSamples: number = 3;
  private graphGuardLiveRefStableRel: number = 0.08;
  private livePoolSumRing: number[] = [];
  private lastLivePoolSumHs: number = 0;
  // Controls how many tick labels are shown on the left hashrate Y axis (Chart.js 'maxTicksLimit').
  private hashrateYAxisMaxTicks: number = 5;
  private hashrateYAxisMinStepThs: number = 0.005;
  private tempYAxisMinStepC: number = 2;
  // Chunk size for the history drainer
  private chunkSizeDrainer: number = 100;
  // --- Rendering smoothing (visual only)
  // Applies to the 1min hashrate dataset. This does not modify data, only the curve rendering.
  // Rule: high point density => higher tension, low density => lower tension.
  private hashrate1mSmoothingCfg = {
    enabled: true,
    // median point interval thresholds (ms)
    fastIntervalMs: 6000,
    mediumIntervalMs: 12000,
    // tensions
    tensionFast: 0.60,
    tensionMedium: 0.25,
    tensionSlow: 0.20,
    // interpolation (keeps curve monotone; avoids overshoot)
    cubicInterpolationMode: 'monotone' as const,
  };

  private applyDatasetRenderOrder(datasets: any[]): void {
    const hr1m: any = datasets?.[0];
    const hr10m: any = datasets?.[1];
    const hr1h: any = datasets?.[2];
    const hr1d: any = datasets?.[3];

    const vr: any = datasets?.[4];
    const asic: any = datasets?.[5];

    // Smaller order renders first (further "behind")
    if (hr1m) hr1m.order = 0;

    // Temps in front of 1m; ASIC behind VR
    if (asic) asic.order = 1;
    if (vr) vr.order = 2;

    // 10m / 1h / 1d in front
    if (hr10m) hr10m.order = 10;
    if (hr1h) hr1h.order = 11;
    if (hr1d) hr1d.order = 12;
  }

  private applyHashrate1mSmoothing(): void {
    const ds: any = (this.chartData?.datasets && this.chartData.datasets.length) ? this.chartData.datasets[0] : null;
    if (!ds) return;

    const cfg = this.hashrate1mSmoothingCfg;
    if (!cfg || !cfg.enabled) {
      ds.tension = 0;
      try { delete (ds as any).cubicInterpolationMode; } catch {}
      return;
    }

    const labels = this.dataLabel;
    let medianIntervalMs = 0;

    if (labels && labels.length >= 3) {
      const diffs: number[] = [];
      const n = Math.min(60, labels.length - 1);
      for (let i = labels.length - n; i < labels.length; i++) {
        const d = labels[i] - labels[i - 1];
        if (Number.isFinite(d) && d > 0) diffs.push(d);
      }
      medianIntervalMs = diffs.length ? valuePillsMedian(diffs) : 0;
    }

    let tension = cfg.tensionSlow;
    if (medianIntervalMs && medianIntervalMs <= cfg.fastIntervalMs) tension = cfg.tensionFast;
    else if (medianIntervalMs && medianIntervalMs <= cfg.mediumIntervalMs) tension = cfg.tensionMedium;

    ds.tension = tension;
    ds.cubicInterpolationMode = cfg.cubicInterpolationMode;
  }

  private setHashrateYAxisLabelCount(count: number): void {
    const n = Math.max(2, Math.min(30, Math.round(Number(count))));
    this.hashrateYAxisMaxTicks = n;

    try {
      const scales: any = (this.chartOptions as any)?.scales;
      if (scales?.y?.ticks) {
        scales.y.ticks.maxTicksLimit = n;
      }
    } catch {}

    try {
      const chart: any = this.chart as any;
      if (chart?.options?.scales?.y?.ticks) {
        chart.options.scales.y.ticks.maxTicksLimit = n;
        chart.update('none');
      }
    } catch {}
  }

  protected readonly NbTrigger = NbTrigger;

  private chart?: Chart;
  private themeSubscription?: Subscription;
  private chartInitialized = false;
  private _info: any;
  private timeFormatListener: any;

  private wasLoaded = false;
  private saveLock = false;

  public info$: Observable<ISystemInfo>;
  public quickLink$: Observable<string | undefined>;
  public fallbackQuickLink$!: Observable<string | undefined>;
  public expectedHashRate$: Observable<number | undefined>;

  public chartOptions: any;
  public dataLabel: number[] = [];
  public dataData: number[] = [];
  public dataData1m: number[] = [];
  public dataData10m: number[] = [];
  public dataData1h: number[] = [];
  public dataData1d: number[] = [];
  public dataVregTemp: number[] = [];
  public dataAsicTemp: number[] = [];
  public chartData?: any;

  public historyDrainRunning = false;
  private historyDrainSub?: Subscription;

  public hasChipTemps: boolean = false;
  public viewMode: 'gauge' | 'bars' = 'bars'; // default to bars

  private localStorageKey = 'chartData_exp';
  private timestampKey = 'lastTimestamp_exp'; // Key to store lastTimestamp
  private tempViewKey = 'tempViewMode_exp';
  private legendVisibilityKey = 'chartLegendVisibility_exp';
  private minHistoryTsKey = 'minHistoryTimestampMs_exp';

  public isDualPool: boolean = false;

  private historyMinTimestampMs: number | null = null;
  private graphGuardState
    = new Map<string, { prev?: number; suspectDir?: -1 | 1; suspectCount: number; window: number[] }>();
  // History drain rendering (to avoid "laggy" incremental build)
  private historyDrainRenderThrottleMs: number = 500;
  private historyDrainUseThrottledRender: boolean = true;
  private historyDrainLastRenderMs: number = 0;
  private historyDrainRenderTimer: any = null;
  private historyDrainRenderPending: boolean = false;
  private suppressChartUpdatesDuringHistoryDrain = false;
  // Debug/test: allow toggling spike-guard for hashrate series (default: enabled)
  private enableHashrateSpikeGuard: boolean = true;
  public debugSpikeGuard: boolean = false;
  public debugPillsLayout: boolean = false;

  // Adaptive axis padding so lines don't stick to frame; tweak here.
  private axisPadCfg = {
    hashrate: {
      windowPoints: 180,     // last N points to consider
      padPct: 0.06,          // fallback padding (symmetrical) as % of range
      padPctTop: 0.05,       // extra headroom above range (as % of range)
      padPctBottom: 0.07,    // extra room below range (as % of range)
      minPadThs: 0.03,       // minimum padding in TH/s (keep small so flat curves don't look "stuck")
      flatPadPctOfMax: 0.005,// if range ~0, use max*X (keep small; too large flattens visible variation)
      maxPadPctOfMax: 0.25,  // cap padding to avoid over-zooming out
    },
    temp: {
      windowPoints: 180,
      padPct: 0.10,
      minPadC: 1.5,
      flatPadC: 2.0,
      maxPadC: 8.0,
    }
  };

  private debugAxisPadding: boolean = false;
  private readonly axisPadOverrideEnabledKey: string = '__nerdCharts_axisPaddingOverrideEnabled';
  private readonly axisPadStorageKey: string = '__nerdCharts_axisPadding';

  ngAfterViewChecked(): void {
    // Ensure chart is initialized only once when the canvas becomes available
    if (!this.chartInitialized && this.ctx && this.ctx.nativeElement) {
      this.chartInitialized = true; // Prevent re-initialization
      this.initChart();
    }
  }

  private initChart(): void {
    this.chart = new Chart(this.ctx.nativeElement.getContext('2d')!, {
      type: 'line',
      data: this.chartData,
      options: this.chartOptions,
    });
    // Restore legend visibility
    const saved = this.localStorageGet(this.legendVisibilityKey);
    if (saved) {
      const visibility = JSON.parse(saved);
      visibility.forEach((hidden: boolean, i: number) => {
        if (hidden) {
          this.chart.getDatasetMeta(i).hidden = true;
        }
      });
      this.chart.update();
    }

    try {
      const flagKey = '__nerdCharts_clearChartHistoryOnce';
      if (this.localStorageGet(flagKey) === '1') {
        this.localStorageRemove(flagKey);
        this.clearChartHistoryInternal(false);
      }
    } catch {}

    this.loadChartData();
    if (this._info?.history) {
      if (this.dataLabel.length === 0) {
        this.importHistoricalDataChunked(this._info.history);
      } else {
        this.importHistoricalData(this._info.history);
      }
    }
  }

  constructor(
    private themeService: NbThemeService,
    private systemService: SystemService,
    private translateService: TranslateService,
    private localStorage: LocalStorageService
  ) {
    const documentStyle = getComputedStyle(document.documentElement);
    const bodyStyle = getComputedStyle(document.body);
    const textColor = bodyStyle.getPropertyValue('--card-text-color');
    const textColorSecondary = bodyStyle.getPropertyValue('--card-text-color');

    // Load persisted view mode early (falls vorhanden)
    const persisted = this.localStorage.getItem(this.tempViewKey);
    if (persisted === 'gauge' || persisted === 'bars') {
      this.viewMode = persisted as 'gauge' | 'bars';
    }

    // Load optional min-history timestamp (used after debug clear to prevent immediate refill)
    try {
      const v = Number(this.localStorageGet(this.minHistoryTsKey));
      if (Number.isFinite(v) && v > 0) {
        this.historyMinTimestampMs = v;
      }
    } catch {}

    this.chartData = {
      labels: [],
      datasets: [
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1M'),
          data: this.dataData1m,
          yAxisID: 'y',
          fill: 'start',
          backgroundColor: (context: any) => this.hr1mAreaGradient(context),
          borderColor: this.hrColor(1),
          pill: { bg: this.hrColor(1) },
          tension: this.hashrate1mSmoothingCfg.tensionFast,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 2.1
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_10M'),
          data: this.dataData10m,
          yAxisID: 'y',
          fill: false,
          backgroundColor: this.hrColor(0),
          borderColor: this.hrColor(0.9),
          tension: .4,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 1.8,
          borderDash: [1, 4],
          borderCapStyle: 'round'
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1H'),
          data: this.dataData1h,
          yAxisID: 'y',
          fill: false,
          backgroundColor: this.hrColor(0),
          borderColor: this.hrColor(0.8),
          tension: .4,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 1.8,
          borderDash: [8, 2]
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1D'),
          data: this.dataData1d,
          yAxisID: 'y',
          fill: false,
          backgroundColor: this.hrColor(0),
          borderColor: this.hrColor(0.8),
          tension: .4,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 1.8,
          borderDash: [14, 8]
        },
        {
          type: 'line',
          label: this.translateService.instant('PERFORMANCE.VR_TEMP_LEGEND'),
          data: this.dataVregTemp,
          yAxisID: 'y_temp',
          fill: true,
          borderColor: '#2DA8B7',
          backgroundColor: (context: any) => this.vrTempAreaGradient(context),
          tension: .4,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 1.4
        },
        {
          type: 'line',
          label: this.translateService.instant('PERFORMANCE.ASIC_TEMP_LEGEND'),
          data: this.dataAsicTemp,
          yAxisID: 'y_temp',
          fill: true,
          borderColor: '#C84847',
          backgroundColor: (context: any) => this.asicTempAreaGradient(context),
          tension: .4,
          cubicInterpolationMode: 'monotone',
          pointRadius: 0,
          borderWidth: 1.3
        }
      ]
    };

    this.applyDatasetRenderOrder(this.chartData.datasets as any[]);

    this.chartOptions = {
      animation: false,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          labels: {
            color: textColor,
            sort: (a: any, b: any) => a.datasetIndex - b.datasetIndex
          },
          onClick: (evt, legendItem, legend) => {
            const chart = legend.chart;
            const index = legendItem.datasetIndex;
            const meta = chart.getDatasetMeta(index);

            // Toggle
            meta.hidden = meta.hidden === null ? !chart.data.datasets[index].hidden : null;

            chart.update();

            // Persist
            const visibility = chart.data.datasets.map((ds, i) =>
              chart.getDatasetMeta(i).hidden ? true : false
            );
            this.localStorageSet(this.legendVisibilityKey, JSON.stringify(visibility));
          }
        },
        tooltip: {
          callbacks: {
            title: (context: any) => {
              const date = new Date(context[0].parsed.x);
              const format = this.localStorage.getItem('timeFormat') === '12h';
              return format
                ? date.toLocaleString('en-US', { hour: 'numeric', minute: '2-digit', hour12: true, month: 'short', day: 'numeric' })
                : date.toLocaleString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false, month: 'short', day: 'numeric' });
            },
            label: (x: any) => {
              if (x?.dataset?.yAxisID === 'y_temp') {
                return `${x.dataset.label}: ${Number(x.raw).toFixed(1)} °C`;
              }
              return `${x.dataset.label}: ${HashSuffixPipe.transform(x.raw)}`;
            }
          }
        },
        valuePills: {
          enabled: true,
          paddingXPx: 7, // +~5px wider pills
          paddingYPx: 6,  // +~2px taller pills
          hashratePillDatasetIndex: 0, // 1m pill only
          debug: this.debugPillsLayout,
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
            unit: 'hour',
            displayFormats: {
              hour: this.localStorage.getItem('timeFormat') === '12h' ? 'h:mm A' : 'HH:mm'
            }
          },
          ticks: {
            color: textColorSecondary
          },
          grid: {
            color: '#80808080',//surfaceBorder,
            drawBorder: false,
            display: true
          }
        },
        y: {
          ticks: {
            color: textColorSecondary,
            maxTicksLimit: this.hashrateYAxisMaxTicks,
            autoSkip: false,
            includeBounds: true,
            callback: (value: number) => HashSuffixPipe.transform(value)
          },
          grid: {
            display: false,
            drawBorder: false
          }
        },
        y_temp: {
          position: "right",
          // min/max are set dynamically from latest temps (±3°C)
          ticks: {
            color: textColorSecondary,
            maxTicksLimit: this.hashrateYAxisMaxTicks,
            callback: (value: number) => `${Math.round(Number(value))} °C`
          },
          grid: {
            color: '#80808080',//surfaceBorder,
            drawBorder: false
          }
        }
      }
    };

    this.info$ = interval(5000).pipe(
      startWith(0), // Immediately start the interval observable
      exhaustMap(() => {
        let storedLastTimestamp = this.getStoredTimestamp();
        try {
          const forcedStart = Number(this.localStorageGet('__nerdCharts_forceStartTimestampMs'));
          if (Number.isFinite(forcedStart) && forcedStart > 0) {
            storedLastTimestamp = forcedStart;
            this.localStorageRemove('__nerdCharts_forceStartTimestampMs');
          }
        } catch {}
        const currentTimestamp = new Date().getTime();
        const oneHourAgo = currentTimestamp - 3600 * 1000;

        // Cap the startTimestamp to be at most one hour ago
        let startTimestamp = storedLastTimestamp ? Math.max(storedLastTimestamp + 1, oneHourAgo) : oneHourAgo;

        return this.systemService.getInfo(startTimestamp, this.chunkSizeDrainer).pipe(
          catchError(err => {
            console.error('[HomeComponent] getInfo polling error', err);
            // Skip this tick, keep last good value and continue polling.
            return EMPTY;
          })
        );
      }),
      tap(info => {
        if (!info) {
          return;
        }
        this._info = info;
        try {
          const flagKey = '__nerdCharts_clearChartHistoryOnce';
          if (this.localStorageGet(flagKey) === '1') {
            this.localStorageRemove(flagKey);
            this.clearChartHistoryInternal(false);
          }
        } catch {}

        if (!this.chart) {
          return info;
        }
        // Only drain on cold start (no cached points yet)
        if (this.dataLabel.length === 0) {
          this.importHistoricalDataChunked(info.history);
        } else {
          this.importHistoricalData(info.history);
        }
      }),
      map(info => {
        if (!info) {
          return SystemService.defaultInfo(); // Return empty object if no info
        }
        info.minVoltage = parseFloat(info.minVoltage.toFixed(1));
        info.maxVoltage = parseFloat(info.maxVoltage.toFixed(1));
        info.minPower = parseFloat(info.minPower.toFixed(1));
        info.maxPower = parseFloat(info.maxPower.toFixed(1));
        info.power = parseFloat(info.power.toFixed(1));
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));

        // Clamp power and voltage values between their min and max
        //info.power = Math.max(info.minPower, Math.min(info.maxPower, info.power));
        //info.voltage = Math.max(info.minVoltage, Math.min(info.maxVoltage, info.voltage));

        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));
        info.temp = parseFloat(info.temp.toFixed(1));
        info.vrTemp = parseFloat(info.vrTemp.toFixed(1));
        info.overheat_temp = parseFloat(info.overheat_temp.toFixed(1));

        this.isDualPool = (info.stratum?.activePoolMode ?? 0) === 1;
        const chipTemps = info?.asicTemps ?? [];
        this.hasChipTemps =
          Array.isArray(chipTemps) &&
          chipTemps.length > 0 &&
          chipTemps.some(v => v != null && !Number.isNaN(Number(v)) && Number(v) !== 0);

        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.expectedHashRate$ = this.info$.pipe(map(info => {
      if (!info) return 0; // Return 0 if no info
      return Math.floor(info.frequency * ((info.smallCoreCount * info.asicCount) / 1000));
    }));

    this.quickLink$ = this.info$.pipe(
      map(info => this.getQuickLink(info.stratumURL, info.stratumUser))
    );

    this.fallbackQuickLink$ = this.info$.pipe(
      map(info => this.getQuickLink(info.fallbackStratumURL, info.fallbackStratumUser))
    );
  }

  /**
   * Returns a pool-specific dashboard / stats URL for the given stratum endpoint.
   *
   * The function delegates to the shared quicklink helper which:
   * - normalizes the stratum URL (supports stratum+tcp://, host:port, host)
   * - extracts the wallet / address from the stratum user
   * - maps known pools to their corresponding web dashboards
   *
   * If no known pool matches, a normalized URL representation of the stratum
   * endpoint is returned as a fallback.
   *
   * @param stratumURL  Stratum pool URL or host
   * @param stratumUser Stratum user string (wallet[.worker])
   * @returns A pool-specific dashboard URL or `undefined` if input is empty
   */
  public getQuickLink(stratumURL: string, stratumUser: string): string | undefined {
    return getQuickLink(stratumURL, stratumUser);
  }

  /**
   * Indicates whether the given stratum pool supports ICMP ping.
   *
   * Some pools intentionally block or ignore ping requests.
   * This helper centralizes pool-specific exceptions.
   *
   * @param stratumURL Stratum pool URL or host
   * @returns `true` if ping is supported, otherwise `false`
   */
  public supportsPing(stratumURL: string): boolean {
    return supportsPing(stratumURL);
  }

  private readonly poolIconErrorCache = new Set<string>();

  /**
   * Resolves the icon URL for a given pool host.
   *
   * Logic:
   * - Uses the existing pool registry / quicklink resolution via `getPoolIconUrl`
   * - If the pool host previously failed to load an icon (favicon or registry icon),
   *   the default pool icon is returned immediately
   * - This guarantees a valid icon for:
   *   - local pools
   *   - registered pools
   *   - unknown public pools
   *
   * @param host Pool hostname
   * @returns URL to the pool icon or the default pool icon
   */
  public poolIconUrl(host: string | undefined | null): string {
    const key = (host ?? '').trim().toLowerCase();
    if (!key) return DEFAULT_POOL_ICON_URL;

    if (this.poolIconErrorCache.has(key)) {
      return isLocalHost(key) ? DEFAULT_POOL_ICON_URL : DEFAULT_EXTERNAL_POOL_ICON_URL;
    }

    return resolvePoolIconUrl(key);
  }

  /**
   * Handles icon load errors for pool icons.
   *
   * When a favicon or registry-provided icon cannot be loaded (e.g. 404, CORS),
   * this method:
   * - stores the host in an internal error cache
   * - replaces the broken image with the default pool icon
   * - prevents repeated failing network requests for the same pool
   *
   * This ensures graceful fallback behavior for unknown public pools.
   *
   * @param evt Image error event
   * @param host Pool hostname associated with the icon
   */
  public onPoolIconError(evt: Event, host: string | undefined | null): void {
    const key = (host ?? '').trim().toLowerCase();
    if (key) this.poolIconErrorCache.add(key);

    const img = evt.target as HTMLImageElement | null;
    if (!img) return;

    const fallback = isLocalHost(key)
      ? DEFAULT_POOL_ICON_URL
      : DEFAULT_EXTERNAL_POOL_ICON_URL;

    if (img.src.includes(fallback)) return;

    img.src = fallback;
  }

private installNerdChartsDebugHooks(): void {
  const g: any = globalThis as any;
  g.__nerdCharts = g.__nerdCharts || {};

  const clearFlagKey = '__nerdCharts_clearChartHistoryOnce';
  const forceStartKey = '__nerdCharts_forceStartTimestampMs';

  // Clear all in-browser chart history once (persisted + in-memory on next load)
  g.__nerdCharts.clearChartHistoryOnce = () => {
    try {
      this.localStorageSet(clearFlagKey, '1');
      // Ensure the next load does NOT immediately re-fill from API history (debug helper)
      const seed = Date.now() - 30000;
      this.localStorageSet(forceStartKey, String(seed));
      this.localStorageSet('__nerdCharts_minHistoryTimestampMs', String(seed));
      this.localStorageSet('lastTimestamp', String(seed));
    } catch (e) {
      console.warn('[nerdCharts] clearChartHistoryOnce failed', e);
    }
  };

  // Clear immediately (no reload needed)
  g.__nerdCharts.clearChartHistoryNow = () => {
    try {
      this.clearChartHistoryInternal(true);
    } catch (e) {
      console.warn('[nerdCharts] clearChartHistoryNow failed', e);
    }
  };

  // Debug: tweak adaptive axis padding via console (applied immediately on next chart update)
  g.__nerdCharts.setAxisPadding = (cfg: any) => {
    const persist = !!(cfg && typeof cfg === 'object' && cfg.persist === true);
    this.setAxisPadding(cfg, persist);
  };

  // Debug: set max tick labels on the hashrate Y axis (Chart.js maxTicksLimit).
  g.__nerdCharts.setHashrateTicks = (n: number) => {
    const v = Math.round(Number(n));
    if (!Number.isFinite(v)) return;
    this.setHashrateYAxisLabelCount(v);
    try { this.chart?.update?.('none'); } catch { try { this.chart?.update?.(); } catch {} }
  };

  // Debug: set minimum tick step on the hashrate Y axis in TH/s (affects tick density).
  g.__nerdCharts.setHashrateMinTickStep = (ths: number) => {
    const v = Number(ths);
    if (!Number.isFinite(v) || v <= 0) return;
    this.hashrateYAxisMinStepThs = v;
    this.updateAxesScaleAdaptive();
    try { this.chart?.update?.('none'); } catch { try { this.chart?.update?.(); } catch {} }
  };

  // Debug: dump current hashrate axis bounds and tick settings to explain why tick counts look a certain way.
  g.__nerdCharts.dumpAxisScale = () => {
    try {
      const y: any = (this.chartOptions.scales as any).y || {};
      const ticks: any = y.ticks || {};
      return {
        yMin: y.min,
        yMax: y.max,
        maxTicksLimit: ticks.maxTicksLimit,
        stepSize: ticks.stepSize,
        hashrateYAxisMaxTicks: this.hashrateYAxisMaxTicks,
        hashrateYAxisMinStepThs: this.hashrateYAxisMinStepThs
      };
    } catch (e: any) {
      return { error: String(e) };
    }
  };

  // Debug: force a render flush for history drain (useful if you switched modes).
  g.__nerdCharts.flushHistoryDrainRender = () => {
    try { this.flushHistoryDrainRenderFinal(); } catch {}
  };

  // Debug: list available commands (once enabled).
  g.__nerdCharts.list = () => Object.keys(g.__nerdCharts).sort();

  // Debug: help text (overrides bootstrap help with full listing once enabled)
  g.__nerdCharts.help = () => ({
    bootstrap: {
      enable: 'enable(persist?: boolean)',
      disable: 'disable(clearPersist?: boolean)'
    },
    history: {
      clearChartHistoryNow: 'clearChartHistoryNow()',
      clearChartHistoryOnce: 'clearChartHistoryOnce(); location.reload()',
      simulateHistoryBackfill: "simulateHistoryBackfill({ chunkSize?: number, chunkDelayMs?: number })",
      flushHistoryDrainRender: 'flushHistoryDrainRender()'
    },
    rendering: {
      setHistoryDrainRenderMode: "setHistoryDrainRenderMode('throttle' | 'final')",
      setHistoryDrainRenderThrottleMs: 'setHistoryDrainRenderThrottleMs(ms: number)'
    },
    axes: {
      setAxisPadding: 'setAxisPadding({ hashPadPctTop?, hashPadPctBottom?, hashMinPadThs?, hashFlatPadPctOfMax?, hashMaxPadPctOfMax?, tempPadPct?, tempMinPadC?, debug?, persist? })',
      saveAxisPadding: 'saveAxisPadding()',
      disableAxisPaddingOverride: 'disableAxisPaddingOverride(); location.reload()',
      setHashrateTicks: 'setHashrateTicks(n: number)'
    }
  });

  g.__nerdCharts.setHashrateTicks = (n: any) => {
    this.setHashrateYAxisLabelCount(Number(n));
  };

  g.__nerdCharts.saveAxisPadding = () => {
    this.saveAxisPaddingOverrides();
  };

  g.__nerdCharts.disableAxisPaddingOverride = () => {
    try {
      window?.localStorage?.removeItem(this.axisPadOverrideEnabledKey);
  } catch (e) {
      console.warn('[nerdCharts] axis padding override disable failed', e);
    }
  };

  g.__nerdCharts.setHashrateTicks = (n: any) => {
    this.setHashrateYAxisLabelCount(n);
  return this.hashrateYAxisMaxTicks;
  };
}

private installNerdChartsDebugBootstrap(): void {
  const g: any = globalThis as any;
  const key = '__nerdCharts_debugMode';

  // Always expose a small bootstrap object.
  const obj: any = (g.__nerdCharts && typeof g.__nerdCharts === 'object') ? g.__nerdCharts : {};
  g.__nerdCharts = obj;

  if (typeof obj.enable !== 'function') {
    obj.enable = (persist?: boolean) => {
      try { if (persist) window?.localStorage?.setItem(key, '1'); } catch {}
      if (!obj.__enabled) {
        this.installNerdChartsDebugHooks();
        obj.__enabled = true;
      }
      return g.__nerdCharts;
    };
  }

  if (typeof obj.disable !== 'function') {
    obj.disable = (clearPersist?: boolean) => {
      try { if (clearPersist) window?.localStorage?.removeItem(key); } catch {}
      return g.__nerdCharts;
    };
  }

  if (typeof obj.help !== 'function') {
    obj.help = () => ({
      enable: 'enable(persist?: boolean)',
      disable: 'disable(clearPersist?: boolean)',
      note: 'Call enable() to install full debug hooks. If persist is true, debug mode will auto-enable after reload.'
    });
  }

  // Auto-enable if persisted.
  const autoEnabled = (() => {
    try { return window?.localStorage?.getItem(key) === '1'; } catch { return false; }
  })();

  if (autoEnabled) obj.enable(false);
}



  // LocalStorage can throw (privacy mode/quota) and may be unavailable in some environments.
  // Centralize access to keep persistence robust.
  /**
   * Read a value from localStorage safely (guards against privacy/quota errors).
   */
  private localStorageGet(key: string): string | null {
    try {
      return localStorage.getItem(key);
    } catch {
      return null;
    }
  }

  /**
   * Write a value to localStorage safely (no-ops if storage is unavailable).
   */
  private localStorageSet(key: string, value: string): void {
    try {
      localStorage.setItem(key, value);
    } catch {
      // Ignore storage errors (e.g., privacy mode/quota).
    }
  }

  /**
   * Remove a localStorage key safely (ignores storage access errors).
   */
  private localStorageRemove(key: string): void {
    try {
      localStorage.removeItem(key);
    } catch {
      // Ignore storage errors.
    }
  }

ngOnInit() {
    this.installNerdChartsDebugBootstrap();
    this.loadAxisPaddingOverrides();
    this.themeSubscription = this.themeService.getJsTheme().subscribe(() => {
      this.updateThemeColors();
    });

    // Listen for timeFormat changes
    this.timeFormatListener = () => {
      this.updateTimeFormat();
    };
    window.addEventListener('timeFormatChanged', this.timeFormatListener);

    // Expose reliable wipe function for settings toggle
    (window as any).__nerdCharts = (window as any).__nerdCharts || {};
    (window as any).__nerdCharts.clearChartHistoryInternal = () => {
      try {
        (this as any).clearChartHistoryInternal();
      } catch {
        // ignore
      }
    };
    // If a wipe was requested before the experimental dashboard was loaded, do it now once.
    try {
      if (localStorage.getItem('__pendingChartHistoryWipe') === '1') {
        localStorage.removeItem('__pendingChartHistoryWipe');
        (this as any).clearChartHistoryInternal();
      }
    } catch {
      // ignore
    }
}

  ngOnDestroy(): void {
    // Destroy Chart.js instance to release canvas resources and internal listeners.
    if (this.chart) {
      this.chart.destroy();
      this.chart = undefined;
    }

    // Clean up theme subscription to avoid memory leaks when navigating away.
    this.themeSubscription?.unsubscribe();
    this.historyDrainSub?.unsubscribe();
    if (this.timeFormatListener) {
      window.removeEventListener('timeFormatChanged', this.timeFormatListener);
    }
  }

  public updateTimeFormat(): void {
    const timeFormat = this.localStorage.getItem('timeFormat') === '12h' ? 'h:mm A' : 'HH:mm';
    if (this.chartOptions.scales?.x?.time?.displayFormats) {
      this.chartOptions.scales.x.time.displayFormats.hour = timeFormat;

      // Update tooltip format as well
      this.chartOptions.plugins.tooltip.callbacks.title = (context: any) => {
        const date = new Date(context[0].parsed.x);
        const format = this.localStorage.getItem('timeFormat') === '12h';
        return format
          ? date.toLocaleString('en-US', { hour: 'numeric', minute: '2-digit', hour12: true, month: 'short', day: 'numeric' })
          : date.toLocaleString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false, month: 'short', day: 'numeric' });
      };

      if (this.chart) {
        this.chart.update();
      }
    }
  }

  private importHistoricalData(data: any) {
    // relative to absolute time stamps
    this.updateChartData(data);

    if (data.timestamps && data.timestamps.length) {
      const lastRel = data.timestamps[data.timestamps.length - 1];
      const base = Number(data.timestampBase ?? 0);
      let lastDataTimestampMs = base + Number(lastRel ?? 0);
      if (lastDataTimestampMs > 0 && lastDataTimestampMs < 1000000000000) lastDataTimestampMs *= 1000;
      this.storeTimestamp(lastDataTimestampMs);
    }

    // During history draining we buffer chunks and render/persist once at the end
    if (this.suppressChartUpdatesDuringHistoryDrain) {
      return;
    }

    // remove data that are older than 1h
    this.filterOldData();

    // save data into the local browser storage
    // only if we had loaded it before
    if (this.wasLoaded) {
      this.saveChartData();
    }

    // set flag that we have finished the initial import
    this.updateChart();
  }

  private clearChartData(): void {
    this.dataLabel = [];
    this.dataData1m = [];
    this.dataData10m = [];
    this.dataData1h = [];
    this.dataData1d = [];
    this.dataVregTemp = [];
    this.dataAsicTemp = [];
  }

private graphGuard(key: string, raw: any, relThreshold: number, liveRef?: number): number {
  // Smooths incoming samples while preventing extreme outliers / line breaks.
  const isHash = key.startsWith('hashrate_');
  const isTemp = key.toLowerCase().includes('temp');
  const minValid = (isHash || isTemp) ? 1 : -Infinity;
  const maxValid = isTemp ? 120 : Infinity;

  const current = Number(raw);
  const valid = Number.isFinite(current) && current > minValid && current < maxValid;
  const state = this.graphGuardState.get(key) ?? { suspectCount: 0, window: [] };
  const prev = state.prev;

  // Rolling median fallback (stabilizes cold starts / bad samples)
  const fallback = state.window.length ? valuePillsMedian(state.window) : (prev ?? 0);
  const candidate = valid ? current : fallback;

  if (prev === undefined) {
    let seed = candidate;

    if (isHash && Number.isFinite(liveRef) && Number(liveRef) > 0) {
      const live = Number(liveRef);
      const tol = Math.max(0.05, this.graphGuardLiveRefTolerance);
      const rel = Math.abs(seed - live) / live;

      // Cold start: seed from live to prevent one-sample spikes after restarts/firmware flashes.
      if (!valid || rel > (tol * 2)) seed = live;
    } else if (!valid) {
      seed = fallback;
    }

    state.prev = seed;
    state.window.push(seed);
    if (state.window.length > 9) state.window.shift();
    this.graphGuardState.set(key, state);
    return seed;
  }

  // --- Live-Gate (hashrate only)
  // Reject samples that are wildly off compared to live pool hashrate (prevents impossible spikes).
  if (isHash && Number.isFinite(liveRef) && Number(liveRef) > 0) {
    const live = Number(liveRef);

    // Allow bigger deviation if live itself is stepping strongly (frequency change).
    const liveStep = prev > 0 ? Math.abs(live - prev) / prev : 0;
    const baseGate = 0.25; // 25% default gate
    const effectiveGate = liveStep > 0.20 ? 0.80 : baseGate;

    const liveRel = Math.abs(candidate - live) / live;
    if (liveRel > effectiveGate) {
      const bigStep = valid && prev > 0 && (Math.abs(candidate - prev) / prev) >= this.graphGuardBigStepRel;
      const liveStable = this.isLivePoolSumStable();
      if (liveStable && !bigStep) {
        if (this.debugSpikeGuard) {
          console.warn('[GraphGuard:LiveGate]', key, { prev, candidate, live, liveRel, effectiveGate, liveStable, bigStep });
        }
        // Hold prev to avoid spikes/line breaks.
        state.prev = prev;
        state.window.push(prev);
        if (state.window.length > 9) state.window.shift();
        this.graphGuardState.set(key, state);
        return prev;
      } else if (this.debugSpikeGuard && liveRel > effectiveGate) {
        console.warn('[GraphGuard:LiveGateBypass]', key, { prev, candidate, live, liveRel, effectiveGate, liveStable, bigStep });
      }

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
    // Fast-path over live reference (pool sum):
    // If this is a real change (frequency up/down) and the candidate is close to the live
    // pool-sum hashrate, accept immediately so scaling updates without waiting for confirmations.
    if (isHash && Number.isFinite(liveRef) && Number(liveRef) > 0) {
      const live = Number(liveRef);
      const liveRel = Math.abs(candidate - live) / live;
      const bigStep = relDiff >= this.graphGuardBigStepRel;
      if (bigStep && liveRel <= this.graphGuardLiveRefTolerance) {
        out = candidate;
        fastAccepted = true;
        state.suspectCount = 0;
        state.suspectDir = undefined;
        if (this.debugSpikeGuard) {
          console.warn('[GraphGuard:FastPath]', key, { prev, candidate, live, liveRel });
        }
      }
    }

    if (!fastAccepted) {
      const dir: -1 | 1 = diff >= 0 ? 1 : -1;

      if (state.suspectDir === dir) state.suspectCount += 1;
      else {
        state.suspectDir = dir;
        state.suspectCount = 1;
      }

      // Step confirmation: accept only after 2 consecutive samples in same direction
      if (state.suspectCount >= this.graphGuardConfirmSamples) {
        out = candidate;
        state.suspectCount = 0;
        state.suspectDir = undefined;
      } else {
        // Suppress without line breaks (keep prev)
        out = prev;
      }
    }

    if (this.debugSpikeGuard) {
      console.warn('[GraphGuard]', key, { prev, candidate, out, relDiff });
    }
  } else {
    state.suspectCount = 0;
    state.suspectDir = undefined;

    // Invalid samples (0/NaN) never create breaks → hold prev
    if (!valid) out = prev;
  }

  state.prev = out;
  state.window.push(out);
  if (state.window.length > 9) state.window.shift();

  this.graphGuardState.set(key, state);
  return out;
}

  // Hashrate for pill (and any live reference): ALWAYS from API info, converted to H/s.
  // Supports APIs that may return H/s, GH/s or TH/s depending on firmware.
  private getPoolHashrateHsSum(): number {
    // Hashrate for pill (and any live reference): ALWAYS from pool sums, converted to H/s.
    // getPoolHashrate() returns GH/s, while the chart series values are in H/s.
    try {
      const a = Number(this.getPoolHashrate(0));
      const b = Number(this.getPoolHashrate(1));
      const sumGh = (Number.isFinite(a) ? a : 0) + (Number.isFinite(b) ? b : 0);
      if (!Number.isFinite(sumGh) || sumGh <= 0) return 0;
      const hs = sumGh * 1e9;
      if (Number.isFinite(hs) && hs > 0) {
        this.livePoolSumRing.push(hs);
        if (this.livePoolSumRing.length > 6) this.livePoolSumRing.shift();
      }
      return hs;
    } catch {
      return 0;
    }
  }

  private isLivePoolSumStable(): boolean {
    const n = this.graphGuardLiveRefStableSamples;
    const rel = this.graphGuardLiveRefStableRel;
    const ring = this.livePoolSumRing;
    if (!Array.isArray(ring) || ring.length < n) return false;
    let mn = Infinity;
    let mx = -Infinity;
    // Use last n samples.
    for (let i = ring.length - n; i < ring.length; i++) {
      const v = ring[i];
      if (!Number.isFinite(v) || v <= 0) return false;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    const base = ring[ring.length - 1];
    if (!Number.isFinite(base) || base <= 0) return false;
    return ((mx - mn) / base) <= rel;
  }

private loadAxisPaddingOverrides(): void {
  try {
    if (window?.localStorage?.getItem(this.axisPadOverrideEnabledKey) !== '1') return;
    const raw = this.localStorageGet(this.axisPadStorageKey);
    if (!raw) return;
    const cfg = JSON.parse(raw);
    if (cfg && typeof cfg === 'object') this.setAxisPadding(cfg, false);
  } catch (e) {
    console.warn('[nerdCharts] axis padding overrides load failed', e);
  }
}

private saveAxisPaddingOverrides(): void {
  try {
    window?.localStorage?.setItem(this.axisPadOverrideEnabledKey, '1');
    window?.localStorage?.setItem(this.axisPadStorageKey, JSON.stringify({
      hashPadPct: this.axisPadCfg.hashrate.padPct,
      hashPadPctTop: this.axisPadCfg.hashrate.padPctTop,
      hashPadPctBottom: this.axisPadCfg.hashrate.padPctBottom,
      hashMinPadThs: this.axisPadCfg.hashrate.minPadThs,
      hashFlatPadPctOfMax: this.axisPadCfg.hashrate.flatPadPctOfMax,
      hashMaxPadPctOfMax: this.axisPadCfg.hashrate.maxPadPctOfMax,
      tempPadPct: this.axisPadCfg.temp.padPct,
      tempMinPadC: this.axisPadCfg.temp.minPadC,
      debug: this.debugAxisPadding,
    }));
  } catch (e) {
    console.warn('[nerdCharts] axis padding overrides save failed', e);
  }
}

private setAxisPadding(cfg: any, persist: boolean = false): void {
  if (!cfg || typeof cfg !== 'object') return;

  if (Number.isFinite(cfg.hashPadPct)) this.axisPadCfg.hashrate.padPct = Number(cfg.hashPadPct);
  if (Number.isFinite(cfg.hashPadPctTop)) this.axisPadCfg.hashrate.padPctTop = Number(cfg.hashPadPctTop);
  if (Number.isFinite(cfg.hashPadPctBottom)) this.axisPadCfg.hashrate.padPctBottom = Number(cfg.hashPadPctBottom);
  if (Number.isFinite(cfg.hashMinPadThs)) this.axisPadCfg.hashrate.minPadThs = Number(cfg.hashMinPadThs);
  if (Number.isFinite(cfg.hashFlatPadPctOfMax)) this.axisPadCfg.hashrate.flatPadPctOfMax = Number(cfg.hashFlatPadPctOfMax);
  if (Number.isFinite(cfg.hashMaxPadPctOfMax)) this.axisPadCfg.hashrate.maxPadPctOfMax = Number(cfg.hashMaxPadPctOfMax);
  if (Number.isFinite(cfg.tempPadPct)) this.axisPadCfg.temp.padPct = Number(cfg.tempPadPct);
  if (Number.isFinite(cfg.tempMinPadC)) this.axisPadCfg.temp.minPadC = Number(cfg.tempMinPadC);
  if (typeof cfg.debug === 'boolean') this.debugAxisPadding = cfg.debug;

  if (persist) this.saveAxisPaddingOverrides();

  if (this.debugAxisPadding) console.info('[nerdCharts] axis padding updated', this.axisPadCfg, { debug: this.debugAxisPadding, persist });
  try { this.updateChart(); } catch {}
}

  private updateAxesScaleAdaptive(): void {
    if (!this.chart || !this.chartOptions?.scales) return;

    const finite = (v: any) => typeof v === 'number' && Number.isFinite(v);

    const clamp = (v: number, mn: number, mx: number) => Math.min(mx, Math.max(mn, v));

    const minMax = (vals: any[]) => {
      let mn = Infinity;
      let mx = -Infinity;
      for (const v of vals) {
        if (!finite(v)) continue;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
      }
      if (!Number.isFinite(mn) || !Number.isFinite(mx)) return { mn: undefined, mx: undefined };
      return { mn, mx };
    };

    // Robust range (percentile trim) to prevent single-point spikes from flattening the chart.
    const percentile = (sorted: number[], p: number): number => {
      if (!sorted.length) return NaN;
      const clamped = Math.min(1, Math.max(0, p));
      const idx = (sorted.length - 1) * clamped;
      const lo = Math.floor(idx);
      const hi = Math.ceil(idx);
      if (lo === hi) return sorted[lo];
      const w = idx - lo;
      return sorted[lo] * (1 - w) + sorted[hi] * w;
    };

    const robustMinMax = (vals: any[]) => {
      const nums: number[] = [];
      for (const v of vals) if (finite(v)) nums.push(v as number);
      if (nums.length < 6) return minMax(nums);

      const sorted = nums.slice().sort((a, b) => a - b);
      const raw = { mn: sorted[0], mx: sorted[sorted.length - 1] };

      const loP = nums.length >= 60 ? 0.02 : 0.05;
      const hiP = nums.length >= 60 ? 0.98 : 0.95;

      let mn = percentile(sorted, loP);
      let mx = percentile(sorted, hiP);

      // If trimming would cut away a non-outlier trend, fall back to raw edges.
      // Only treat it as an outlier if it's clearly separated from the trimmed bound.
      const sepUp = (mx > 0) ? (raw.mx - mx) / mx : (raw.mx - mx);
      const sepDn = (mn > 0) ? (mn - raw.mn) / mn : (mn - raw.mn);

      if (!(Number.isFinite(sepUp) && sepUp > 0.12)) mx = raw.mx;
      if (!(Number.isFinite(sepDn) && sepDn > 0.12)) mn = raw.mn;

      if (!Number.isFinite(mn) || !Number.isFinite(mx) || mx <= mn) return raw;
      return { mn, mx };
    };

    // Determine the currently visible X-window (ms). If not set, fall back to full data.
    const labels = this.dataLabel as any[];
    const xOpt = (this.chartOptions.scales as any).x || {};
    const xScale: any = (this.chart as any).scales?.x;
    const xMin = Number.isFinite(Number(xOpt.min)) ? Number(xOpt.min) : (Number.isFinite(Number(xScale?.min)) ? Number(xScale.min) : undefined);
    const xMax = Number.isFinite(Number(xOpt.max)) ? Number(xOpt.max) : (Number.isFinite(Number(xScale?.max)) ? Number(xScale.max) : undefined);

    const lowerBound = (arr: number[], x: number) => {
      let lo = 0, hi = arr.length;
      while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (arr[mid] < x) lo = mid + 1; else hi = mid;
      }
      return lo;
    };
    const upperBound = (arr: number[], x: number) => {
      let lo = 0, hi = arr.length;
      while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (arr[mid] <= x) lo = mid + 1; else hi = mid;
      }
      return lo;
    };

    let idxFrom = 0;
    let idxTo = labels.length;

    if (labels.length > 0 && Number.isFinite(xMin) && Number.isFinite(xMax)) {
      // labels are absolute ms timestamps (sorted)
      idxFrom = lowerBound(labels as any, xMin as any);
      idxTo = upperBound(labels as any, xMax as any);
      if (idxFrom < 0) idxFrom = 0;
      if (idxTo > labels.length) idxTo = labels.length;
      if (idxTo <= idxFrom) {
        idxFrom = 0;
        idxTo = labels.length;
      }
    }

    // Fallback safety: avoid scanning gigantic histories unnecessarily
    const maxScan = 5000;
    if (idxTo - idxFrom > maxScan) {
      idxFrom = Math.max(0, idxTo - maxScan);
    }

    const hashVals: number[] = [];
    const collect = (arr: any[]) => {
      if (!Array.isArray(arr)) return;
      for (let i = idxFrom; i < idxTo && i < arr.length; i++) {
        const v = arr[i];
        if (finite(v)) hashVals.push(v);
      }
    };

    // Include 1min always (pill positioning + ensure it stays inside the plot if data exists)
    collect(this.dataData1m as any[]);

    // Add other visible hashrate series (10m/1h/1d) if enabled by the user.
    const hashSeries: Array<{ i: number; data: any[] }> = [
      { i: 1, data: this.dataData10m as any[] },
      { i: 2, data: this.dataData1h as any[] },
      { i: 3, data: this.dataData1d as any[] },
    ];
    for (const s of hashSeries) {
      if (this.chart.isDatasetVisible(s.i)) collect(s.data);
    }

    const hm = robustMinMax(hashVals);

    const tempVals: number[] = [];
    const collectT = (arr: any[]) => {
      if (!Array.isArray(arr)) return;
      for (let i = idxFrom; i < idxTo && i < arr.length; i++) {
        const v = arr[i];
        if (finite(v)) tempVals.push(v);
      }
    };

    // Add temperature series if enabled by the user (but pills still show even if hidden)
    if (this.chart.isDatasetVisible(4)) collectT(this.dataVregTemp as any[]);
    if (this.chart.isDatasetVisible(5)) collectT(this.dataAsicTemp as any[]);
    // If both temp datasets are hidden but we still have data, keep scales sane by including their values.
    if (!this.chart.isDatasetVisible(4) && !this.chart.isDatasetVisible(5)) {
      collectT(this.dataVregTemp as any[]);
      collectT(this.dataAsicTemp as any[]);
    }

    const tm = minMax(tempVals);

    // --- Hashrate (H/s): adaptive padding, but based on the actually visible X-window
    if (hm.mn !== undefined && hm.mx !== undefined) {
      const cfgH = this.axisPadCfg.hashrate;
      const range = Math.max(1e6, hm.mx - hm.mn); // at least 1 MH/s to avoid degenerate scaling

      const padPctFallback = Number(cfgH?.padPct ?? 0.06);
      const padPctTop = Number(cfgH?.padPctTop ?? padPctFallback);
      const padPctBottom = Number(cfgH?.padPctBottom ?? padPctFallback);
      const minPadThs = Number(cfgH?.minPadThs ?? 0.15);
      const minPadHs = minPadThs * 1e12;
      const flatPadPctOfMax = Number(cfgH?.flatPadPctOfMax ?? 0.03);
      const maxPadPctOfMax = Number(cfgH?.maxPadPctOfMax ?? 0.25);

      const maxAbs = Math.max(1, Math.abs(hm.mx));
      const flatPad = maxAbs * flatPadPctOfMax;

      const padTop = clamp(Math.max(range * padPctTop, minPadHs, flatPad), 0, maxAbs * maxPadPctOfMax);
      const padBottom = clamp(Math.max(range * padPctBottom, minPadHs, flatPad), 0, maxAbs * maxPadPctOfMax);

      const targetMin = hm.mn - padBottom;
      const targetMax = hm.mx + padTop;

      // Fast-path for Y-scale: if live pool-sum hashrate sits outside the current bounds,
      // expand immediately so the 1m chart stays readable during real frequency steps.
      let adjMin = targetMin;
      let adjMax = targetMax;
      const live = this.lastLivePoolSumHs;
      if (Number.isFinite(live) && live > 0) {
        if (live > adjMax) adjMax = live + padTop;
        if (live < adjMin) adjMin = Math.max(0, live - padBottom);
      }

      (this.chartOptions.scales as any).y = (this.chartOptions.scales as any).y || {};
      (this.chartOptions.scales as any).y.min = adjMin;
      (this.chartOptions.scales as any).y.max = adjMax;

      // Enforce "nice" Y-tick spacing to avoid overly granular labels (e.g. 6.24 & 6.25 TH/s).
      // IMPORTANT: Hashrate series values are in H/s, but the UI shows TH/s. Tick step must be set in H/s units.
      // Chart.js otherwise tries to fill maxTicksLimit by shrinking step size when the range is small.
      try {
        const yScale: any = (this.chartOptions.scales as any).y;
        yScale.ticks = yScale.ticks || {};

        const HS_PER_THS = 1e12;
        const rangeHs = Math.max(0, adjMax - adjMin);
        const rangeThs = rangeHs / HS_PER_THS;

        const maxTicks = Math.max(2, Number(this.hashrateYAxisMaxTicks || 7));
        const desiredThs = rangeThs / Math.max(1, maxTicks - 1);

        // "Nice" steps in TH/s; we then convert back to H/s for Chart.js.
        const niceStepsThs = [0.005, 0.01, 0.02,0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10];
        let stepThs = niceStepsThs[niceStepsThs.length - 1];

        // Prefer a step that yields a tick count close to maxTicks (maxTicksLimit is only a maximum).
        // This avoids cases where desiredThs≈0.025 but we jump to 0.05 and end up with ~4 ticks.
        let bestScore = Number.POSITIVE_INFINITY;
        for (const s of niceStepsThs) {
          if (s < this.hashrateYAxisMinStepThs) continue;
          const ticks = Math.floor(rangeThs / s) + 1;
          // Slight penalty when exceeding maxTicks (Chart.js may auto-skip), but still allow it.
          const score = Math.abs(ticks - maxTicks) + (ticks > maxTicks ? 0.25 : 0);
          if (score < bestScore || (score === bestScore && s > stepThs)) {
            bestScore = score;
            stepThs = s;
          }
        }
        if (stepThs < this.hashrateYAxisMinStepThs) stepThs = this.hashrateYAxisMinStepThs;

        const stepHs = stepThs * HS_PER_THS;

        // Align bounds to the step so Chart.js actually uses the requested stepSize (otherwise it may pick its own).
        const minAligned = Math.floor(adjMin / stepHs) * stepHs;
        const maxAligned = Math.ceil(adjMax / stepHs) * stepHs;

        (this.chartOptions.scales as any).y.min = minAligned;
        (this.chartOptions.scales as any).y.max = maxAligned;

        yScale.ticks.maxTicksLimit = maxTicks;
        yScale.ticks.stepSize = stepHs;
      } catch {}

      if (this.debugAxisPadding) {
      }
    } else {
      // If we cannot determine a range, let Chart.js auto-scale.
      (this.chartOptions.scales as any).y = (this.chartOptions.scales as any).y || {};
      delete (this.chartOptions.scales as any).y.min;
      delete (this.chartOptions.scales as any).y.max;
    }

    // --- Temperature (°C): hard limit to +3°C above max and -2°C below min (visible X-window)
    if (tm.mn !== undefined && tm.mx !== undefined) {
      const targetMin = tm.mn - 2;
      const targetMax = tm.mx + 3;

      (this.chartOptions.scales as any).y_temp = (this.chartOptions.scales as any).y_temp || {};
      (this.chartOptions.scales as any).y_temp.min = targetMin;
      (this.chartOptions.scales as any).y_temp.max = targetMax;

      // Enforce "nice" temperature tick spacing to avoid unhelpful 1°C adjacent labels (e.g. 60°C & 61°C).
      try {
        const yTemp: any = (this.chartOptions.scales as any).y_temp;
        yTemp.ticks = yTemp.ticks || {};

        const maxTicks = Math.max(2, Number(this.hashrateYAxisMaxTicks || 7));
        const rangeC = Math.max(0, targetMax - targetMin);
        const desired = rangeC / Math.max(1, maxTicks - 1);

        const niceStepsC = [0.5, 1, 2, 5, 10];
        let stepC = niceStepsC[niceStepsC.length - 1];
        for (const s of niceStepsC) {
          if (s >= desired) { stepC = s; break; }
        }
        if (stepC < this.tempYAxisMinStepC) stepC = this.tempYAxisMinStepC;

        // Align to step to ensure Chart.js honors stepSize.
        const minAligned = Math.floor(targetMin / stepC) * stepC;
        const maxAligned = Math.ceil(targetMax / stepC) * stepC;

        yTemp.min = minAligned;
        yTemp.max = maxAligned;

        yTemp.ticks.maxTicksLimit = maxTicks;
        yTemp.ticks.stepSize = stepC;
      } catch {}

      if (this.debugAxisPadding) {
      }
    } else {
      (this.chartOptions.scales as any).y_temp = (this.chartOptions.scales as any).y_temp || {};
      delete (this.chartOptions.scales as any).y_temp.min;
      delete (this.chartOptions.scales as any).y_temp.max;
    }
  }


private updateChartData(data: any): void {
    if (!data) return;

    const baseTimestamp = Number(data.timestampBase ?? 0);

    const timestamps: number[] = Array.isArray(data.timestamps) ? data.timestamps : [];
    const h1m: number[] = Array.isArray(data.hashrate_1m) ? data.hashrate_1m : [];
    const h10m: number[] = Array.isArray(data.hashrate_10m) ? data.hashrate_10m : [];
    const h1h: number[] = Array.isArray(data.hashrate_1h) ? data.hashrate_1h : [];
    const h1d: number[] = Array.isArray(data.hashrate_1d) ? data.hashrate_1d : [];
    const vreg: number[] = Array.isArray(data.vregTemp) ? data.vregTemp : [];
    const asic: number[] = Array.isArray(data.asicTemp) ? data.asicTemp : [];

    const n = Math.min(timestamps.length, h1m.length, h10m.length, h1h.length, h1d.length, vreg.length, asic.length);
    if (n <= 0) return;

    const livePoolSum = this.getPoolHashrateHsSum();

    this.lastLivePoolSumHs = livePoolSum;

    // History is appended sorted; last element is the highest timestamp (cheaper than Math.max(...)).
    const lastTimestamp = this.dataLabel.length > 0 ? this.dataLabel[this.dataLabel.length - 1] : -Infinity;

    // Filter new data to include only timestamps greater than the lastTimestamp
    const newData: any[] = [];
    for (let i = 0; i < n; i++) {
      let tsAbs = Number(timestamps[i]) + baseTimestamp;
      // API safety: accept both seconds and milliseconds
      if (tsAbs > 0 && tsAbs < 1000000000000) tsAbs *= 1000;
      if (!Number.isFinite(tsAbs)) continue;
      if (this.historyMinTimestampMs != null && tsAbs < this.historyMinTimestampMs) continue;
      if (tsAbs < lastTimestamp) continue;

      newData.push({
        timestamp: tsAbs,
        hashrate_1m: Number(h1m[i]) * 1000000000.0 / 100.0,
        hashrate_10m: Number(h10m[i]) * 1000000000.0 / 100.0,
        hashrate_1h: Number(h1h[i]) * 1000000000.0 / 100.0,
        hashrate_1d: Number(h1d[i]) * 1000000000.0 / 100.0,
        vregTemp: Number(vreg[i]) / 100.0,
        asicTemp: Number(asic[i]) / 100.0,
      });
    }

    newData.sort((a, b) => a.timestamp - b.timestamp);


    // Append only new data (spike-guarded, line-break free)
    // If we push duplicates, Chart.js draws a vertical segment at the same X ("treppenhaus").
    for (const entry of newData) {
      const lastIdx = this.dataLabel.length - 1;
      if (lastIdx >= 0 && this.dataLabel[lastIdx] === entry.timestamp) {
        this.dataData1m[lastIdx] = this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1m', entry.hashrate_1m, 0.01, livePoolSum) : entry.hashrate_1m;
        this.dataVregTemp[lastIdx] = this.graphGuard('vregTemp', entry.vregTemp, 0.35);
        this.dataAsicTemp[lastIdx] = this.graphGuard('asicTemp', entry.asicTemp, 0.35);
        this.dataData10m[lastIdx] = this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_10m', entry.hashrate_10m, 0.02, livePoolSum) : entry.hashrate_10m;
        this.dataData1h[lastIdx] = this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1h', entry.hashrate_1h, 0.08, livePoolSum) : entry.hashrate_1h;
        this.dataData1d[lastIdx] = this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1d', entry.hashrate_1d, 0.10, livePoolSum) : entry.hashrate_1d;
        continue;
      }

      this.dataLabel.push(entry.timestamp);
      this.dataData1m.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1m', entry.hashrate_1m, 0.01, livePoolSum) : entry.hashrate_1m);
      this.dataVregTemp.push(this.graphGuard('vregTemp', entry.vregTemp, 0.35));
      this.dataAsicTemp.push(this.graphGuard('asicTemp', entry.asicTemp, 0.35));
      this.dataData10m.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_10m', entry.hashrate_10m, 0.02, livePoolSum) : entry.hashrate_10m);
      this.dataData1h.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1h', entry.hashrate_1h, 0.08, livePoolSum) : entry.hashrate_1h);
      this.dataData1d.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1d', entry.hashrate_1d, 0.10, livePoolSum) : entry.hashrate_1d);
    }
  }

  private loadChartData(): void {
    // Allow persistence from now on (even if there is no data yet on first run).
    this.wasLoaded = true;

    const raw = this.localStorageGet(this.localStorageKey);
    if (!raw) return;

    try {
      const parsed = JSON.parse(raw);

      // Validate minimal required schema: labels + the 4 hashrate series.
      const labels = Array.isArray(parsed?.labels) ? parsed.labels : null;

      const d1m = Array.isArray(parsed?.dataData1m) ? parsed.dataData1m : null;
      const d10m = Array.isArray(parsed?.dataData10m) ? parsed.dataData10m : null;
      const d1h = Array.isArray(parsed?.dataData1h) ? parsed.dataData1h : null;
      const d1d = Array.isArray(parsed?.dataData1d) ? parsed.dataData1d : null;

      if (!labels || !d1m || !d10m || !d1h || !d1d) {
        throw new Error('Invalid chartData storage shape (missing required series)');
      }

      const targetLen = labels.length;

      const normalizeToLen = (arr: number[] | null, len: number): number[] => {
        if (!arr) return new Array(len).fill(Number.NaN);
        if (arr.length === len) return arr;
        if (arr.length > len) return arr.slice(0, len);
        return arr.concat(new Array(len - arr.length).fill(Number.NaN));
      };

      // Optional series: allow missing keys (backward compatible).
      const vreg = Array.isArray(parsed?.dataVregTemp) ? parsed.dataVregTemp : null;
      const asic = Array.isArray(parsed?.dataAsicTemp) ? parsed.dataAsicTemp : null;

      this.dataLabel = labels;
      this.dataData1m = normalizeToLen(d1m, targetLen);
      this.dataData10m = normalizeToLen(d10m, targetLen);
      this.dataData1h = normalizeToLen(d1h, targetLen);
      this.dataData1d = normalizeToLen(d1d, targetLen);
      this.dataVregTemp = normalizeToLen(vreg, targetLen);
      this.dataAsicTemp = normalizeToLen(asic, targetLen);

      // Keep chartData in sync with the restored arrays.
      if (this.chartData) {
        this.updateChart();
      }
    } catch (err) {
      console.warn('[HomeComponent] Failed to load chartData from localStorage (keeping it untouched).', err);

      // Reset in-memory only, but do NOT wipe storage automatically.
      this.dataLabel = [];
      this.dataData1m = [];
      this.dataData10m = [];
      this.dataData1h = [];
      this.dataData1d = [];
      this.dataVregTemp = [];
      this.dataAsicTemp = [];

      if (this.chartData) {
        this.updateChart();
      }
    }
  }

  private sanitizeLoadedHistory(): void {
    const len = this.dataLabel.length;
    if (!len) return;

    // Re-run loaded points through the spike-guard so cached spikes can't persist.
    this.graphGuardState.clear();

    const out1m: number[] = [];
    const out10m: number[] = [];
    const out1h: number[] = [];
    const out1d: number[] = [];
    const outVreg: number[] = [];
    const outAsic: number[] = [];

    for (let i = 0; i < len; i++) {
      out1m.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1m', this.dataData1m[i], 0.01) : this.dataData1m[i]);
      outVreg.push(this.graphGuard('vregTemp', this.dataVregTemp[i], 0.35));
      outAsic.push(this.graphGuard('asicTemp', this.dataAsicTemp[i], 0.35));
      out10m.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_10m', this.dataData10m[i], 0.02) : this.dataData10m[i]);
      out1h.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1h', this.dataData1h[i], 0.08) : this.dataData1h[i]);
      out1d.push(this.enableHashrateSpikeGuard ? this.graphGuard('hashrate_1d', this.dataData1d[i], 0.10) : this.dataData1d[i]);
    }

    this.dataData1m = out1m;
    this.dataData10m = out10m;
    this.dataData1h = out1h;
    this.dataData1d = out1d;
    this.dataVregTemp = outVreg;
    this.dataAsicTemp = outAsic;
  }

  private saveChartData(): void {
    if (this.saveLock) {
      return;
    }
    const dataToSave = {
      labels: this.dataLabel,
      dataData1m: this.dataData1m,
      dataData10m: this.dataData10m,
      dataData1h: this.dataData1h,
      dataData1d: this.dataData1d,
      dataVregTemp: this.dataVregTemp,
      dataAsicTemp: this.dataAsicTemp,
    };
    this.localStorageSet(this.localStorageKey, JSON.stringify(dataToSave));
  }

  private filterOldData(): void {
    const now = new Date().getTime();
    const cutoff = now - 3600 * 1000;

    // Fast trim without O(n^2) shift loops (important for long gaps / history backfill)
    let idx = 0;
    const len = this.dataLabel.length;
    while (idx < len && this.dataLabel[idx] < cutoff) idx++;

    if (idx > 0) {
      this.dataLabel = this.dataLabel.slice(idx);
      this.dataData1m = this.dataData1m.slice(idx);
      this.dataData10m = this.dataData10m.slice(idx);
      this.dataData1h = this.dataData1h.slice(idx);
      this.dataData1d = this.dataData1d.slice(idx);
      this.dataVregTemp = this.dataVregTemp.slice(idx);
      this.dataAsicTemp = this.dataAsicTemp.slice(idx);
    }

    if (this.dataLabel.length) {
      this.storeTimestamp(this.dataLabel[this.dataLabel.length - 1]);
    }
  }

  private storeTimestamp(timestamp: number): void {
    if (this.saveLock) {
      return;
    }
    this.localStorageSet(this.timestampKey, timestamp.toString());
  }

  private getStoredTimestamp(): number | null {
    const storedTimestamp = this.localStorageGet(this.timestampKey);
    if (storedTimestamp) {
      const timestamp = parseInt(storedTimestamp, 10);
      return timestamp;
    }
    return null;
  }


private updateTempScaleFromLatest(): void {
  // Keep temp axis zoomed: latest temps ±3°C (makes fluctuations visible).
  const lastV = valuePillsFindLastFinite(this.dataVregTemp as any[]);
  const lastA = valuePillsFindLastFinite(this.dataAsicTemp as any[]);
  if (lastV == null && lastA == null) return;

  const vals = [lastV, lastA].filter(v => v != null && Number.isFinite(Number(v))) as number[];
  if (!vals.length) return;

  const minLast = Math.min(...vals);
  const maxLast = Math.max(...vals);

  const min = Math.floor(minLast - 3);
  const max = Math.ceil(maxLast + 3);

  if (this.chartOptions?.scales?.y_temp) {
    this.chartOptions.scales.y_temp.min = min;
    this.chartOptions.scales.y_temp.max = max;
  }
}

  private updateChart() {
    this.chartData.labels = this.dataLabel;
    this.chartData.datasets[0].data = this.dataData1m;
    this.chartData.datasets[1].data = this.dataData10m;
    this.chartData.datasets[2].data = this.dataData1h;
    this.chartData.datasets[3].data = this.dataData1d;
    this.chartData.datasets[4].data = this.dataVregTemp;
    this.chartData.datasets[5].data = this.dataAsicTemp;

    this.updateTempScaleFromLatest();

    // Update hashrate pill display value from live pool sum.
    if (this.chartOptions?.plugins?.valuePills) {
      this.chartOptions.plugins.valuePills.hashrateDisplayValue = this.getPoolHashrateHsSum();
    }

    if (!this.chart) {
      return;
    }

    this.updateAxesScaleAdaptive();
    this.applyHashrate1mSmoothing();

    this.chart.update();
  }

  private updateThemeColors(): void {
    const bodyStyle = getComputedStyle(document.body);
    const textColor = bodyStyle.getPropertyValue('--card-text-color').trim();

    // Update chart options
    this.chartOptions.plugins.legend.labels.color = textColor;
    this.chartOptions.scales.x.ticks.color = textColor;
    this.chartOptions.scales.x.grid.color = '#80808080';
    this.chartOptions.scales.y.ticks.color = textColor;
    this.chartOptions.scales.y.grid.color = '#80808080';
    this.chartOptions.scales.y_temp.ticks.color = textColor;
    this.chartOptions.scales.y_temp.grid.color = '#80808080';

    // Update and redraw the chart
    if (this.chart) {
      this.chart.options = this.chartOptions;
      this.chart.update();
    }
  }

  // Toggle only if feature exists, then persist
  public onTempViewClick(event: Event): void {
    // Prevent toggling when chip temps aren't available
    if (!this.hasChipTemps) return;

    // Toggle mode
    this.viewMode = this.viewMode === 'bars' ? 'gauge' : 'bars';

    // Persist to local storage
    this.localStorage.setItem(this.tempViewKey, this.viewMode);
  }

  public poolBadgeStatus(): string {
    const stratum = this._info.stratum;

    if (stratum === undefined) {
      return "warning";
    }

    const pool = stratum.pools[0];

    if (!pool.connected) {
      return 'danger';
    }

    // Failover mode: same behavior as before
    return stratum.usingFallback ? 'warning' : 'success';
  }

  public getPoolPercent(idx: 0 | 1): number {
    const balance = this._info.stratum.poolBalance ?? 50;
    return idx === 0 ? balance : 100 - balance;
  }

  public showPoolBadge(idx: 0 | 1): boolean {
    return this.getPoolPercent(idx) > 0;
  }

  public poolBadgeLabel(): string {
    const stratum = this._info.stratum;

    if (stratum === undefined) {
      return this.translateService.instant('HOME.DISCONNECTED');
    }
    const pool = stratum.pools[0];

    if (!pool.connected) {
      return this.translateService.instant('HOME.DISCONNECTED');
    }
    return stratum.usingFallback
      ? this.translateService.instant('HOME.FALLBACK_POOL')
      : this.translateService.instant('HOME.PRIMARY_POOL');
  }

  public dualPoolBadgeLabel(i: 0 | 1) {
    const percent = this.getActiveBalance(i);
    return `Pool ${i + 1} (${percent} %)`;
  }

  public dualPoolBadgeTooltip(i: 0 | 1) {
    const stratum = this._info.stratum;
    const pool = stratum.pools[i];
    const connected = pool.connected;
    const diffErr = pool.poolDiffErr;

    if (diffErr) {
      return this.translateService.instant('HOME.SHARE_TOO_SMALL');
    }

    if (connected) {
      return this.translateService.instant('HOME.CONNECTED');
    }

    return this.translateService.instant('HOME.DISCONNECTED');;
  }

  public dualPoolBadgeStatus(i: 0 | 1) {
    const pool = this._info.stratum.pools[i];
    const connected = pool.connected;
    const diffErr = pool.poolDiffErr;

    if (diffErr) {
      return "warning";
    }

    if (connected) {
      return "success";
    }

    return "danger";
  }

  public getPoolHashrate(i: 0 | 1) {
    const balance = this.getActiveBalance(i);
    return this._info.hashRate * balance / 100.0;
  }

  public getActiveBalance(i: 0 | 1) {
    const stratum = this._info.stratum;
    const connected = stratum.pools.map(p => p.connected);
    const balance = stratum.poolBalance;

    // If neither pool is connected
    if (!connected[0] && !connected[1]) {
      return 0;
    }

    // If both pools are connected
    if (connected[0] && connected[1]) {
      return i === 0 ? balance : 100 - balance;
    }

    // Only one pool is connected → return 100 for that pool, 0 for the other
    return connected[i] ? 100 : 0;
  }


  public getPoolInfo(i?: 0 | 1): IPool {
    const stratum = this._info.stratum;

    // failover logic, "current" pool
    if (i === undefined) {
      const useFallback = stratum?.usingFallback ?? false;
      const base = stratum?.pools[useFallback ? 1 : 0] ?? {};

      return {
        ...base,
        host: useFallback ? this._info.fallbackStratumURL : this._info.stratumURL,
        port: useFallback ? this._info.fallbackStratumPort : this._info.stratumPort,
        user: useFallback ? this._info.fallbackStratumUser : this._info.stratumUser,
      };
    }

    // explicit pool 0 / 1 (dual pool)
    const base = stratum.pools[i];

    return {
      ...base,
      host: i === 0 ? this._info.stratumURL : this._info.fallbackStratumURL,
      port: i === 0 ? this._info.stratumPort : this._info.fallbackStratumPort,
      user: i === 0 ? this._info.stratumUser : this._info.fallbackStratumUser,
    };
  }

  public getPoolCardIndices(): (0 | 1 | undefined)[] {
    return (this._info.stratum?.activePoolMode ?? 0) === 0 ? [undefined] : [0, 1];
  }

  private clearChartHistoryInternal(updateChartNow: boolean): void {
    this.historyDrainSub?.unsubscribe();
    this.historyDrainRunning = false;

    this.clearChartData();
    this.graphGuardState.clear();

    // Clear persisted history
    this.localStorageRemove(this.localStorageKey);
    this.localStorageRemove(this.timestampKey);

    // Prevent immediate refill with old history (API/local) after clearing.
    // Seed a minimum timestamp slightly in the past to allow the very next sample through.
    const seed = Date.now() - 30000;
    this.historyMinTimestampMs = seed;
    try {
      this.localStorageSet(this.minHistoryTsKey, String(seed));
      this.localStorageSet(this.timestampKey, String(seed));
    } catch {}

    if (updateChartNow) {
      this.updateChart();
    }
  }

  // edge case where chart data in the browser is not consistent
  // this happens when adding new charts
  private validateOrResetHistory(): void {
    const lenLabels = this.dataLabel.length;
    const len1m = this.dataData1m.length;
    const len10m = this.dataData10m.length;
    const len1h = this.dataData1h.length;
    const len1d = this.dataData1d.length;
    const lenVregTemp = this.dataVregTemp.length;
    const lenAsicTemp = this.dataAsicTemp.length;

    const lengths = [lenLabels, len1m, len10m, len1h, len1d, lenVregTemp, lenAsicTemp];

    // If all arrays have the same length, everything is fine.
    const allEqual = lengths.every(l => l === lengths[0]);
    if (allEqual) return;

    // If labels are missing but any series has data, we cannot safely recover.
    const anySeriesHasData = [len1m, len10m, len1h, len1d, lenVregTemp, lenAsicTemp].some(l => l > 0);
    if (lenLabels === 0 && anySeriesHasData) {
      console.warn('[History] Labels missing while series data exists; clearing in-memory only.', {
        lenLabels, len1m, len10m, len1h, len1d, lenVregTemp, lenAsicTemp,
      });

      // Clear in-memory only; do not wipe persisted storage here.
      this.dataLabel = [];
      this.dataData1m = [];
      this.dataData10m = [];
      this.dataData1h = [];
      this.dataData1d = [];
      this.dataVregTemp = [];
      this.dataAsicTemp = [];
      return;
    }

    console.warn('[History] Inconsistent lengths detected; repairing series to match labels.', {
      lenLabels, len1m, len10m, len1h, len1d, lenVregTemp, lenAsicTemp,
    });

    const normalizeToLen = (arr: number[], targetLen: number): number[] => {
      if (arr.length === targetLen) return arr;
      if (arr.length > targetLen) return arr.slice(0, targetLen);

      // Pad missing values with NaN so Chart.js will skip those points.
      const pad = new Array(targetLen - arr.length).fill(Number.NaN);
      return arr.concat(pad);
    };

    // Normalize all series to labels length.
    this.dataData1m = normalizeToLen(this.dataData1m, lenLabels);
    this.dataData10m = normalizeToLen(this.dataData10m, lenLabels);
    this.dataData1h = normalizeToLen(this.dataData1h, lenLabels);
    this.dataData1d = normalizeToLen(this.dataData1d, lenLabels);
    this.dataVregTemp = normalizeToLen(this.dataVregTemp, lenLabels);
    this.dataAsicTemp = normalizeToLen(this.dataAsicTemp, lenLabels);

    // Do not clear persisted history and do not force a reload.
    // The next regular save will persist the repaired shape.
  }

  public rejectRate(id: number) {
    const stratum = this._info.stratum;

    if (stratum === undefined) {
      return 0;
    }

    const rejected = stratum.pools[id].rejected;
    const accepted = stratum.pools[id].accepted;

    if (accepted == 0 && rejected == 0) {
      return 0.0;
    }
    return rejected / (accepted + rejected) * 100;
  }

  private requestHistoryDrainRender(): void {
    if (!this.historyDrainUseThrottledRender) return;
    if (!this.chart) return;

    this.historyDrainRenderPending = true;

    const now = Date.now();
    const elapsed = now - this.historyDrainLastRenderMs;
    const dueIn = Math.max(0, this.historyDrainRenderThrottleMs - elapsed);

    if (this.historyDrainRenderTimer != null) {
      return;
    }

    this.historyDrainRenderTimer = window.setTimeout(() => {
      this.historyDrainRenderTimer = null;
      if (!this.historyDrainRenderPending) return;
      this.historyDrainRenderPending = false;
      this.historyDrainLastRenderMs = Date.now();
      this.updateChart();
    }, dueIn);
  }

  private flushHistoryDrainRenderFinal(): void {
    if (this.historyDrainRenderTimer != null) {
      clearTimeout(this.historyDrainRenderTimer);
      this.historyDrainRenderTimer = null;
    }
    this.historyDrainRenderPending = false;
    this.historyDrainLastRenderMs = Date.now();

    this.filterOldData();
    if (this.wasLoaded) {
      this.saveChartData();
    }
    this.updateChart();
  }

  private getLastAbsTimestampFromHistory(history: any): number | null {
    if (!history?.timestamps?.length) return null;
    const maxRel = Math.max(...history.timestamps);
    let ts = Number(history.timestampBase ?? 0) + Number(maxRel ?? 0);
    if (ts > 0 && ts < 1000000000000) ts *= 1000;
    return ts;
  }

  private importHistoricalDataChunked(history: any): void {
    // Import current chunk using your existing logic
    this.importHistoricalData(history);
    this.requestHistoryDrainRender();

    // If no more chunks -> done
    if (this.debugSpikeGuard || this.debugAxisPadding) console.log("has more: " + history?.hasMore);
    if (!history?.hasMore) {
      this.historyDrainRunning = false;
      return;
    }

    // If we are already draining, don't start a second chain
    if (this.historyDrainRunning) {
      return;
    }

    this.historyDrainRunning = true;
    this.suppressChartUpdatesDuringHistoryDrain = true;

    const startNext = this.getLastAbsTimestampFromHistory(history);
    if (startNext === null) {
      this.historyDrainRunning = false;
      this.suppressChartUpdatesDuringHistoryDrain = false;
      return;
    }

    const fetchNext = (startTs: number) => {
      this.historyDrainSub = this.systemService.getInfo(startTs, this.chunkSizeDrainer).pipe(take(1)).subscribe({
        next: (info) => {
          const h = info?.history;
          if (!h) {
            this.historyDrainRunning = false;
            this.suppressChartUpdatesDuringHistoryDrain = false;
            return;
          }

          // Import next chunk
          this.importHistoricalData(h);
          this.requestHistoryDrainRender();

          // Continue?
          if (h.hasMore) {
            const lastAbs = this.getLastAbsTimestampFromHistory(h);
            if (lastAbs === null) {
              this.historyDrainRunning = false;
              this.suppressChartUpdatesDuringHistoryDrain = false;
              return;
            }
            fetchNext(lastAbs + 1);
          } else {
            this.historyDrainRunning = false;
            this.suppressChartUpdatesDuringHistoryDrain = false;
            this.flushHistoryDrainRenderFinal();
          }
        },
        error: (err) => {
          console.error('[HistoryDrain] failed', err);
          // Ensure the UI can continue updating after a drain error.
          this.historyDrainRunning = false;
          this.suppressChartUpdatesDuringHistoryDrain = false;
          // If we buffered render updates, flush once so the user still sees partial progress.
          this.flushHistoryDrainRenderFinal();
        }
      });
    };

    fetchNext(startNext + 1);
  }
}

// Small numeric helper (used for axis padding and pill layout).
function clamp(v: number, lo: number, hi: number): number {
  if (!Number.isFinite(v)) return lo;
  return Math.min(Math.max(v, lo), hi);
}

type ValuePillsSide = 'left' | 'right';

interface ValuePillsDatasetOverrides {
  plotGapPx?: number;
  outerPaddingPx?: number;
  bg?: string;
  fg?: string;
}

interface ValuePillsPluginOptions {
  enabled?: boolean;
  // Hashrate pill: display value can be overridden (e.g. live pool sum)
  hashrateDisplayValue?: number;
  // Hashrate pill: which dataset index to anchor to (default: 0 / 1min)
  hashratePillDatasetIndex?: number;
  fontSizePx?: number;
  fontFamily?: string;
  paddingXPx?: number;
  paddingYPx?: number;
  borderRadiusPx?: number;
  minGapPx?: number;
  defaultPlotGapPx?: number;
  defaultOuterPaddingPx?: number;
  minTotalGapPx?: number;
  debug?: boolean;
}

interface ValuePill {
  datasetIndex: number;
  side: ValuePillsSide;
  axisId: string;
  text: string;
  value: number;
  targetY: number;
  // Optional Y-clamp (e.g. temp pills limited to ±3°C)
  clampMinY?: number;
  clampMaxY?: number;
  x: number;
  y: number;
  w: number;
  h: number;
  plotGapPx: number;
  outerPaddingPx: number;
  bg: string;
  fg: string;
  yUpLimit?: number;
  yDownLimit?: number;
}

function valuePillsMedian(values: number[]): number {
  const arr = values.filter(v => Number.isFinite(v)).slice().sort((a, b) => a - b);
  if (!arr.length) return 0;
  const mid = Math.floor(arr.length / 2);
  return arr.length % 2 ? arr[mid] : (arr[mid - 1] + arr[mid]) / 2;
}

function valuePillsFindLastFinite(data: any[]): number | null {
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

function valuePillsApplyMinTotalGap(plotGapPx: number, outerPaddingPx: number, minTotalGapPx: number): { plotGapPx: number; outerPaddingPx: number } {
  if (plotGapPx + outerPaddingPx >= minTotalGapPx) return { plotGapPx, outerPaddingPx };
  return { plotGapPx: Math.max(plotGapPx, minTotalGapPx - outerPaddingPx), outerPaddingPx };
}

function valuePillsFormatValue(axisId: string, value: number, decimalsOverride?: number): string {
  // Pills: explicit decimals (Hash: 2, Temp: 1)
  if (axisId === 'y_temp') {
    const d = Number.isFinite(decimalsOverride) ? Number(decimalsOverride) : 1;
    return `${value.toFixed(d)} °C`;
  }
  // Hashrate pills: always display in TH/s (series values are in H/s)
  const d = Number.isFinite(decimalsOverride) ? Number(decimalsOverride) : 2;
  const v = Number(value);
  if (!Number.isFinite(v)) return `0.00 TH/s`;
  return `${(v / 1e12).toFixed(d)} TH/s`;
}

function valuePillsRoundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
  const radius = Math.max(0, Math.min(r, w / 2, h / 2));
  ctx.beginPath();
  ctx.moveTo(x + radius, y);
  ctx.arcTo(x + w, y, x + w, y + h, radius);
  ctx.arcTo(x + w, y + h, x, y + h, radius);
  ctx.arcTo(x, y + h, x, y, radius);
  ctx.arcTo(x, y, x + w, y, radius);
  ctx.closePath();
}

function valuePillsComputePills(chart: any, opts: ValuePillsPluginOptions, measureOnly: boolean): ValuePill[] {
  const o = opts ?? {};
  const enabled = o.enabled !== false;
  if (!enabled) return [];

  const fontSizePx = o.fontSizePx ?? 11;
  const fontFamily = o.fontFamily ?? 'system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif';
  const paddingXPx = o.paddingXPx ?? 8;
  const paddingYPx = o.paddingYPx ?? 4;
  const borderRadiusPx = o.borderRadiusPx ?? 10;
  const minTotalGapPx = o.minTotalGapPx ?? 12;

  const ctx: CanvasRenderingContext2D = chart.ctx;
  ctx.save();
  ctx.font = `bold ${fontSizePx}px ${fontFamily}`;

  const pills: ValuePill[] = [];

  chart.data.datasets.forEach((ds: any, datasetIndex: number) => {
    const axisId = ds.yAxisID || 'y';
    // Only show pills for Hashrate (1min) and temperatures
    if (!(axisId === 'y_temp' || datasetIndex === 0)) return;
    const side: ValuePillsSide = axisId === 'y_temp' ? 'right' : 'left';

    const overrides: ValuePillsDatasetOverrides = ds.pill || {};
    let plotGapPx = Number.isFinite(overrides.plotGapPx) ? Number(overrides.plotGapPx) : (o.defaultPlotGapPx ?? 6);
    let outerPaddingPx = Number.isFinite(overrides.outerPaddingPx) ? Number(overrides.outerPaddingPx) : (o.defaultOuterPaddingPx ?? 6);
    ({ plotGapPx, outerPaddingPx } = valuePillsApplyMinTotalGap(plotGapPx, outerPaddingPx, minTotalGapPx));

    const lastValue = valuePillsFindLastFinite(ds.data as any[]);
    if (lastValue === null) return;

    // Hashrate pill: text from live pool sum, Y-position from 1m chart history.
    const displayValue = (axisId === 'y' && Number.isFinite((o as any).hashrateDisplayValue))
      ? Number((o as any).hashrateDisplayValue)
      : lastValue;

    const text = valuePillsFormatValue(axisId, displayValue, axisId === 'y_temp' ? 1 : 2);
    const textW = ctx.measureText(text).width;
    const w = Math.ceil(textW + paddingXPx * 2);
    const h = Math.ceil(fontSizePx + paddingYPx * 2);

    pills.push({
      datasetIndex,
      side,
      axisId,
      text,
      value: lastValue,
      targetY: 0,
      x: 0,
      y: 0,
      w,
      h,
      plotGapPx,
      outerPaddingPx,
      bg: (overrides.bg || ds.borderColor || ds.backgroundColor || '#333') as string,
      fg: (overrides.fg || '#ffffff') as string,
    });
  });

  ctx.restore();

  if (measureOnly) return pills;

  // Vertical anchoring: dataset-coupled (last valid point)
  pills.forEach(p => {
    const scale = chart.scales?.[p.axisId];
    if (!scale?.getPixelForValue) return;
    p.targetY = scale.getPixelForValue(p.value);
    p.y = p.targetY;

    // Temperature pills: allow only a small, scale-coupled vertical offset (+3°C / -2°C)
    if (p.axisId === 'y_temp') {
      const yUp = scale.getPixelForValue(p.value + 3);
      const yDown = scale.getPixelForValue(p.value - 2);
      p.yUpLimit = Math.min(yUp, yDown);
      p.yDownLimit = Math.max(yUp, yDown);
    }
  });

  return pills;
}

function valuePillsStackVertically(pills: ValuePill[], chartArea: any, opts: ValuePillsPluginOptions): void {
  if (!pills.length) return;

  const o = opts ?? {};
  const baseMinGap = o.minGapPx ?? 4;

  // sort by target y
  pills.sort((a, b) => a.targetY - b.targetY);

  const topBound = chartArea.top;
  const bottomBound = chartArea.bottom;

  const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

  // Allowed range per pill (keeps pills inside frame and, for temps, within a small scale-range)
  pills.forEach((p, i) => {
    const halfH = p.h / 2;
    const top = topBound + p.outerPaddingPx + halfH;
    const bot = bottomBound - p.outerPaddingPx - halfH;

    let minY = top;
    let maxY = bot;

    // Temperature pills: limit movement strictly to +3°C up and -2°C down (scale-coupled)
    if (p.axisId === 'y_temp' && Number.isFinite(p.yUpLimit) && Number.isFinite(p.yDownLimit)) {
      minY = Math.max(minY, p.yUpLimit!);
      maxY = Math.min(maxY, p.yDownLimit!);
    }

    // stash on object for later passes
    (p as any)._minY = minY;
    (p as any)._maxY = maxY;
    p.y = clamp(p.targetY, minY, maxY);
  });

  // Constraint relaxation (few passes) so we can push down and, if needed, pull up within ranges
  for (let iter = 0; iter < 6; iter++) {
    let changed = false;

    for (let i = 1; i < pills.length; i++) {
      const prev = pills[i - 1];
      const cur = pills[i];
      const minGap = Math.max(baseMinGap, Math.round((prev.h + cur.h) * 0.1)); // height-aware
      const minDist = prev.h / 2 + cur.h / 2 + minGap;
      const need = prev.y + minDist;

      if (cur.y < need) {
        const ny = clamp(need, (cur as any)._minY, (cur as any)._maxY);
        if (ny !== cur.y) {
          cur.y = ny;
          changed = true;
        }
      }
    }

    for (let i = pills.length - 2; i >= 0; i--) {
      const cur = pills[i];
      const next = pills[i + 1];
      const minGap = Math.max(baseMinGap, Math.round((next.h + cur.h) * 0.1)); // height-aware
      const minDist = next.h / 2 + cur.h / 2 + minGap;
      const need = next.y - minDist;

      if (cur.y > need) {
        const ny = clamp(need, (cur as any)._minY, (cur as any)._maxY);
        if (ny !== cur.y) {
          cur.y = ny;
          changed = true;
        }
      }
    }

    if (!changed) break;
  }

  // final clamp
  pills.forEach(p => {
    p.y = clamp(p.y, (p as any)._minY, (p as any)._maxY);
  });
}

function valuePillsEllipsizeToFit(ctx: CanvasRenderingContext2D, text: string, maxTextW: number): string {
  if (ctx.measureText(text).width <= maxTextW) return text;
  const ell = '…';
  let lo = 0;
  let hi = text.length;
  while (lo < hi) {
    const mid = Math.floor((lo + hi) / 2);
    const candidate = text.slice(0, mid) + ell;
    if (ctx.measureText(candidate).width <= maxTextW) lo = mid + 1;
    else hi = mid;
  }
  const cut = Math.max(0, lo - 1);
  return text.slice(0, cut) + ell;
}

function valuePillsDrawPills(chart: any, opts: ValuePillsPluginOptions, pills: ValuePill[]): void {
  const o = opts ?? {};
  const fontSizePx = o.fontSizePx ?? 11;
  const fontFamily = o.fontFamily ?? 'system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif';
  const paddingXPx = o.paddingXPx ?? 8;
  const paddingYPx = o.paddingYPx ?? 4;
  const borderRadiusPx = o.borderRadiusPx ?? 10;

  const ctx: CanvasRenderingContext2D = chart.ctx;
  const chartArea = chart.chartArea;
  if (!chartArea) return;
  const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

  // group per side and collision-resolve separately
  const left = pills.filter(p => p.side === 'left');
  const right = pills.filter(p => p.side === 'right');

  valuePillsStackVertically(left, chartArea, o);
  valuePillsStackVertically(right, chartArea, o);

  ctx.save();
  ctx.font = `bold ${fontSizePx}px ${fontFamily}`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';

  const drawOne = (p: ValuePill) => {
    const minTotalGapPx = o.minTotalGapPx ?? 12;
    const gap = Math.max(p.plotGapPx + p.outerPaddingPx, minTotalGapPx);
    const outer = p.outerPaddingPx;

    // Reserve a strict band outside the plot area so pills never drift into the chart.
    const bandMin = (p.side === 'left') ? outer : (chartArea.right + gap);
    const bandMax = (p.side === 'left') ? (chartArea.left - gap) : (chart.width - outer);
    const bandW = Math.max(0, bandMax - bandMin);

    // If the pill is wider than the available band, ellipsize to fit the band.
    if (p.w > bandW) {
      const maxTextW = Math.max(0, bandW - paddingXPx * 2);
      p.text = valuePillsEllipsizeToFit(ctx, p.text, maxTextW);
      p.w = Math.ceil(ctx.measureText(p.text).width + paddingXPx * 2);
    }

    const scale = chart.scales?.[p.axisId];
    const fallbackCenter = (p.side === 'left') ? (bandMax - p.w / 2) : (bandMin + p.w / 2);
    const center = scale ? (scale.left + scale.right) / 2 : fallbackCenter;

    let x = clamp(center - p.w / 2, bandMin, bandMax - p.w);

    p.x = x;

    // draw pill
    ctx.fillStyle = p.bg;
    valuePillsRoundRect(ctx, p.x, p.y - p.h / 2, p.w, p.h, borderRadiusPx);
    ctx.fill();

    ctx.fillStyle = p.fg;
    ctx.fillText(p.text, p.x + p.w / 2, p.y);
  };

  left.forEach(drawOne);
  right.forEach(drawOne);

  if (o.debug) {
    ctx.save();
    ctx.strokeStyle = '#ff00ff';
    ctx.lineWidth = 1;
    ctx.strokeRect(chartArea.left, chartArea.top, chartArea.right - chartArea.left, chartArea.bottom - chartArea.top);
    ctx.restore();
  }

  ctx.restore();
}

const valuePillsPlugin: Plugin = {
  id: 'valuePills',
  beforeLayout: (chart: any) => {
    const opts = chart.options?.plugins?.valuePills as ValuePillsPluginOptions | undefined;
    if (!opts?.enabled) return;

    // Compute pill sizes early, then reserve scale width via afterFit wrapper (no layout padding bloat).
    const pills = valuePillsComputePills(chart, opts, true);

    const reqByAxis: Record<string, number> = {};
    pills.forEach(p => {
      const minTotalGapPx = (opts.minTotalGapPx ?? 12);
      const gap = Math.max((p.plotGapPx ?? 0) + (p.outerPaddingPx ?? 0), minTotalGapPx);
      // Ensure the scale is wide enough so the full pill text fits (no ellipsis/clipping).
      const req = Math.ceil(p.w + gap + p.outerPaddingPx);
      reqByAxis[p.axisId] = Math.max(reqByAxis[p.axisId] ?? 0, req);
    });
    (chart as any)._valuePillsReqScaleWidth = reqByAxis;

    // Wrap afterFit once per scale to enforce minimum width.
    Object.keys(chart.scales ?? {}).forEach((axisId) => {
      const scale: any = chart.scales[axisId];
      if (!scale || scale._valuePillsAfterFitWrapped) return;

      const original = scale.afterFit?.bind(scale);
      scale.afterFit = () => {
        if (original) original();
        const req = (chart as any)._valuePillsReqScaleWidth?.[axisId];
        if (Number.isFinite(req)) {
          scale.width = Math.max(scale.width, Number(req));
        }
      };
      scale._valuePillsAfterFitWrapped = true;
    });
  },
  afterDatasetsDraw: (chart: any) => {
    const opts = chart.options?.plugins?.valuePills as ValuePillsPluginOptions | undefined;
    if (!opts?.enabled) return;

    const pills = valuePillsComputePills(chart, opts, false);
    const ready = pills.filter(p => Number.isFinite(p.y));
    valuePillsDrawPills(chart, opts, ready);
  },
};

Chart.register(TimeScale, valuePillsPlugin);
