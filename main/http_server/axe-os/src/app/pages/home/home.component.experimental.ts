import {
  Component,
  AfterViewChecked,
  OnInit,
  OnDestroy,
  ElementRef,
  ViewChild,
  HostBinding,
  Renderer2 } from '@angular/core';
import { map,
  Observable,
  Subscription,
  firstValueFrom } from 'rxjs';
import { HashSuffixPipe } from '../../pipes/hash-suffix.pipe';
import { SystemService } from '../../services/system.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { Chart } from 'chart.js';  // Import Chart.js
import { registerHomeChartPlugins } from './plugins';
import { HOME_CFG,
  createAxisPaddingCfg } from './home.cfg';
import {
  findLastFinite,
  HomeChartState,
  HomeChartStorage,
  HomeHistoryDrainer,
  createHomeChartConfig,
  createHomeChart,
  applyHomeChartTheme,
  createSystemInfoPolling$,
  installNerdChartsDebugBootstrap,
  GraphGuard,
  computeXWindow,
  computeHomeChartScales,
  applyAxisBoundsToChartOptions,
  shouldUnlockStartup,
  HomeWarmupMachine,
  shouldInsertRestartCut,
  syncHomeChartDataAndSmoothing,
  getHistoryOldestTimestampMs,
  shouldStartHr1mFromHistory,
  shouldShowZoomWindowLabel,
  clampWindowMs,
  stepWindowMs,
  toggleWindowMs,
  ChartZoomCfg,
  formatZoomWindowLabel,
  updateChartWithZoomAnimation,
} from './chart';

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

// Tile helpers (keep this component as a thin container)
import { maxAsicTemp,
  splitHumanReadable,
  toPct,
  isBarWarn,
  isBarCrit,
  isBarMax,
  BAR_LIMITS,
  poolDiff,
  abbrevMiddle,
  getAsicFrequencyBoundsFromAsic,
  FreqBounds,
  getAsicCoreVoltageBoundsFromAsic,
  VoltBounds,
	shutdownTempC,
	isAsicTempWarn,
	isAsicTempCrit,
  isBarOver,
  isOutsideBand,
  isAtLeast,
  isBetween,
  formatUptime,
  normalizeHomeTileInfo,
  HomeBarDomSync,
  hexToRgba
} from './tiles/utils';
@Component({
  selector: 'app-home-experimental',
  templateUrl: './home.component.experimental.html',
  styleUrls: ['./home.component.experimental.scss']
})

export class HomeExperimentalComponent implements AfterViewChecked, OnInit, OnDestroy {
  @ViewChild('myChart') ctx!: ElementRef<HTMLCanvasElement>;

  // Persisted UI state: chart collapsed (visual-only; data continues tracking).
  public isChartCollapsed: boolean = false;
  private readonly chartCollapsedKey: string = HOME_CFG.storage.keys.chartCollapsed;
  private chartWindowMs: number = HOME_CFG.xAxis.fixedWindowMs;
  private zoomCfg: ChartZoomCfg = {
    minWindowMs: HOME_CFG.xAxis.minWindowMs,
    maxWindowMs: HOME_CFG.xAxis.maxWindowMs,
    zoomStepMs: HOME_CFG.xAxis.zoomStepMs,
  };

  // CSS vars for meter bars (kept in sync with HOME_CFG)
  @HostBinding('style.--bar-fill') barFill: string = HOME_CFG.colors.hashrateBase;
  @HostBinding('style.--bar-track') barTrack: string = HOME_CFG.colors.chartGridColor;
  @HostBinding('style.--asic-temp-pill') asicTempPill: string = HOME_CFG.colors.asicTemp;

  // DOM hook: special-case bar fills without complicating templates
  private currentInputBarMaxWanted: boolean = false;
  private vrTempBarCritWanted: boolean = false;
  private barDomSync: HomeBarDomSync;

  // Track current Nebular theme name so we can apply small light-theme-only overrides.
  private currentThemeName: string = '';

  private applyXWindowToChart(xMinMs: number, xMaxMs: number): void {
    // Update shared chart options (used on theme refresh etc.)
    try {
      const x = (this.chartOptions as any)?.scales?.x;
      if (x) {
        x.min = xMinMs;
        x.max = xMaxMs;
      }
    } catch {}

    // Update the live chart instance options so the viewport changes immediately
    try {
      const x = (this.chart as any)?.options?.scales?.x;
      if (x) {
        x.min = xMinMs;
        x.max = xMaxMs;
      }
    } catch {}
  }

  private setChartWindowMs(nextMs: number): void {
    const next = clampWindowMs(nextMs, this.zoomCfg);
    if (next === this.chartWindowMs) return;
    const prev = this.chartWindowMs;
    this.chartWindowMs = next;
    // Reset sticky temp bounds so the axis re-fits to the new window.
    this.lastTempAxisMin = null;
    this.lastTempAxisMax = null;

    if (next > prev) {
      const oldest = this.dataLabel?.length ? this.dataLabel[0] : null;
      const cutoff = Date.now() - next;
      const needsBackfill = !Number.isFinite(oldest as any) || Number(oldest) > cutoff + 2000;
      if (needsBackfill) {
        void this.reloadHistoryForWindow(next);
      }
    }

    this.updateAxesScaleAdaptive();
    this.syncChartDatasetsAndSmoothing();
    // Force immediate dataset refresh so smoothing changes apply before any next tick.
    try { this.chart?.update?.('none'); } catch {}
    updateChartWithZoomAnimation(this.chart, 160);
  }

  public zoomOut(evt?: Event): void {
    try { (evt?.currentTarget as HTMLElement | null)?.blur?.(); } catch {}
    this.setChartWindowMs(stepWindowMs(this.chartWindowMs, this.zoomCfg.zoomStepMs, this.zoomCfg));
  }

  public zoomIn(evt?: Event): void {
    try { (evt?.currentTarget as HTMLElement | null)?.blur?.(); } catch {}
    this.setChartWindowMs(stepWindowMs(this.chartWindowMs, -this.zoomCfg.zoomStepMs, this.zoomCfg));
  }

  public toggleZoomWindow(evt?: Event): void {
    try { (evt?.currentTarget as HTMLElement | null)?.blur?.(); } catch {}
    this.setChartWindowMs(toggleWindowMs(this.chartWindowMs, this.zoomCfg));
  }

  public get zoomWindowLabel(): string {
    const hourShort = this.translateService.instant('UNITS.HOUR_SHORT');
    const minuteShort = this.translateService.instant('UNITS.MINUTE_SHORT');
    return formatZoomWindowLabel(this.chartWindowMs, hourShort, minuteShort);
  }

  public get showZoomWindowLabel(): boolean {
    return shouldShowZoomWindowLabel(this.chartWindowMs, this.zoomCfg);
  }

  /**
   * Tile helpers exposed to the template.
   * Kept here as public function refs so the HTML can call them.
   */
  public maxAsicTemp = maxAsicTemp;
  public splitHumanReadable = splitHumanReadable;
  public toPct = toPct;
  public isBarWarn = isBarWarn;
  public isBarCrit = isBarCrit;
  public isBarMax = isBarMax;
  public isBarOver = isBarOver;
  public BAR_LIMITS = BAR_LIMITS;

  /**
   * Input Voltage warn-band (yellow) should be data-driven (HOME_CFG) and centralized.
   * We keep the template free of thresholds by routing through this method.
   */
  public isInputVoltageWarn(voltage: any): boolean {
    const band = HOME_CFG.tiles.inputVoltageBand;
    return isOutsideBand(voltage, band.low, band.high);
  }

  /**
   * Input current warning thresholds depend on device max current.
   * - For devices < lowMaxAThreshold: warn/crit at 98% / 99%
   * - For devices >= lowMaxAThreshold: use default warn/crit
   */
  public isInputCurrentWarn(currentA: any, minA: any, maxA: any): boolean {
    const cfg = HOME_CFG.tiles.inputCurrent;
    const max = Number(maxA);
    const useLow = Number.isFinite(max) && max < Number(cfg.lowMaxAThreshold ?? 8);
    const warnRel = useLow ? Number(cfg.lowWarnRel ?? 0.98) : Number(cfg.warnRel ?? 0.94);
    return isBarWarn(currentA, minA, maxA, warnRel);
  }

  public isInputCurrentCrit(currentA: any, minA: any, maxA: any): boolean {
    const cfg = HOME_CFG.tiles.inputCurrent;
    const max = Number(maxA);
    const useLow = Number.isFinite(max) && max < Number(cfg.lowMaxAThreshold ?? 8);
    const critRel = useLow ? Number(cfg.lowCritRel ?? 0.99) : Number(cfg.critRel ?? 0.98);
    return isBarCrit(currentA, minA, maxA, critRel);
  }

  /** Voltage Regulator temperature bands (yellow/red) are configured in HOME_CFG. */
  public isVrTempWarn(vrTempC: any): boolean {
    const band = HOME_CFG.tiles.vrTempBand;
    return isBetween(vrTempC, band.warnC, band.critC);
  }

  public isVrTempCrit(vrTempC: any): boolean {
    const band = HOME_CFG.tiles.vrTempBand;
    return isAtLeast(vrTempC, band.critC);
  }
  // ASIC temperature scaling + warn/crit thresholds (used by ASIC °C + A1/A2… squares)
  public shutdownTempC = shutdownTempC;
  public isAsicTempWarn = isAsicTempWarn;
  public isAsicTempCrit = isAsicTempCrit;

  /**
   * Uptime formatting for the hashrate tile (months/weeks/days/hours/minutes).
   * Always shows minutes.
   */
  public formatUptime = (totalSeconds: number): string =>
    formatUptime(totalSeconds, HOME_CFG.tiles.uptime);

  // ASIC frequency scaling (device-specific)
  private _freqBoundsCacheKey: any = null;
  private _freqBoundsCacheVal: FreqBounds = { min: 0, max: 1 };

  // Snapshot of `/asic` endpoint (same data source as SETTINGS.FREQUENCY)
  private _asicInfo: any = null;
  public asicFreqBounds(info: any): FreqBounds {
    if (info && info === this._freqBoundsCacheKey) return this._freqBoundsCacheVal;
    const bounds = getAsicFrequencyBoundsFromAsic(info, this._asicInfo);
    this._freqBoundsCacheKey = info;
    this._freqBoundsCacheVal = bounds;
    return bounds;
  }
  public asicFreqMax(info: any): number { return this.asicFreqBounds(info).max; }


  // ASIC core-voltage scaling (device-specific, Settings-aligned)
  private _voltBoundsCacheKey: any = null;
  private _voltBoundsCacheVal: VoltBounds = { min: 0.9, max: 1.8 };

  public asicVoltBounds(info: any): VoltBounds {
    if (info && info === this._voltBoundsCacheKey) return this._voltBoundsCacheVal;
    const bounds = getAsicCoreVoltageBoundsFromAsic(info, this._asicInfo);
    this._voltBoundsCacheKey = info;
    this._voltBoundsCacheVal = bounds;
    return bounds;
  }
  public asicVoltMin(info: any): number { return this.asicVoltBounds(info).min; }
  public asicVoltMax(info: any): number { return this.asicVoltBounds(info).max; }

	/**
	 * Backwards-compatible alias used by the template.
	 * (The template calls asicCoreVoltageMax(info) to match the label in the UI.)
	 */
	public asicCoreVoltageMax(info: any): number { return this.asicVoltMax(info); }

  public poolDiff = poolDiff;
  public abbrevMiddle = abbrevMiddle;
  // --- GraphGuard
  // Step-Confirmation: how many consecutive "suspicious" samples in the same direction
  // are required before accepting a step. Increase to 3 to be more conservative.
  private graphGuardConfirmSamples: number = HOME_CFG.graphGuard.cfg.confirmSamples;
  // If a "suspicious" hashrate step matches the live pool-sum reference within this tolerance,
  // accept immediately (so the Y-scale reacts in 1–2 ticks).
  private graphGuardLiveRefTolerance: number = HOME_CFG.graphGuard.cfg.liveRefTolerance;
  // A step >= this relative delta vs previous sample is treated as a likely real change (e.g. freq up/down)
  // and will not be blocked by the live-ref gate. (5s updates -> reacts in ~10s with confirmSamples=2)
  private graphGuardBigStepRel: number = HOME_CFG.graphGuard.cfg.bigStepRel;
  // Live pool-sum stability detector to avoid trusting a single live tick.
  private graphGuardLiveRefStableSamples: number = HOME_CFG.graphGuard.cfg.liveRefStableSamples;
  private graphGuardLiveRefStableRel: number = HOME_CFG.graphGuard.cfg.liveRefStableRel;
  private lastLivePoolSumHs: number = 0;
  // Timestamp of the last inserted NaN break-point (hard cut). Used to avoid collisions with history timestamps
  private lastHardBreakTs: number = 0;
  // Controls how many tick labels are shown on the left hashrate Y axis (Chart.js 'maxTicksLimit').
  private hashrateYAxisMaxTicks: number = HOME_CFG.yAxis.hashrateMaxTicksDefault;
  private hashrateYAxisMinStepThs: number = HOME_CFG.yAxis.minTickSteps.hashrateMinStepThs;
  private tempYAxisMinStepC: number = HOME_CFG.yAxis.minTickSteps.tempMinStepC;
  private lastTempAxisMin: number | null = null;
  private lastTempAxisMax: number | null = null;
  // Chunk size for the history drainer (0 means no limit)
  private chunkSizeDrainer: number = HOME_CFG.historyDrain.chunkSize;
  // --- Rendering smoothing (visual only)
  // Applies to the 1min hashrate dataset. This does not modify data, only the curve rendering.
  // Rule: high point density => higher tension, low density => lower tension.
  private hashrate1mSmoothingCfg = { ...HOME_CFG.smoothing.hashrate1m };

  private setHashrateYAxisLabelCount(count: number): void {
    const clamp = HOME_CFG.yAxis.hashrateTickCountClamp;
    const n = Math.max(clamp.min, Math.min(clamp.max, Math.round(Number(count))));
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
  private chartState: HomeChartState = new HomeChartState();
  private chartStorage: HomeChartStorage;

  // Backward-compatible accessors (keeps the rest of the component diff small)
  public get dataLabel(): number[] { return this.chartState.labels; }
  public set dataLabel(v: number[]) { this.chartState.labels = v; }
  public get dataData(): number[] { return []; }
  public set dataData(_v: number[]) { /* unused */ }
  public get dataData1m(): number[] { return this.chartState.hr1m; }
  public set dataData1m(v: number[]) { this.chartState.hr1m = v; }
  public get dataData10m(): number[] { return this.chartState.hr10m; }
  public set dataData10m(v: number[]) { this.chartState.hr10m = v; }
  public get dataData1h(): number[] { return this.chartState.hr1h; }
  public set dataData1h(v: number[]) { this.chartState.hr1h = v; }
  public get dataData1d(): number[] { return this.chartState.hr1d; }
  public set dataData1d(v: number[]) { this.chartState.hr1d = v; }
  public get dataVregTemp(): number[] { return this.chartState.vregTemp; }
  public set dataVregTemp(v: number[]) { this.chartState.vregTemp = v; }
  public get dataAsicTemp(): number[] { return this.chartState.asicTemp; }
  public set dataAsicTemp(v: number[]) { this.chartState.asicTemp = v; }
  public chartData?: any;
  public historyDrainRunning = false;
  private historyDrainer: HomeHistoryDrainer;
  public hasChipTemps: boolean = false;
  public isDualPool: boolean = false;
  private historyMinTimestampMs: number | null = null;
  // History drain rendering (to avoid "laggy" incremental build)
  private historyDrainRenderThrottleMs: number = HOME_CFG.historyDrain.renderThrottleMs;
  private historyDrainUseThrottledRender: boolean = HOME_CFG.historyDrain.useThrottledRender;
  private suppressChartUpdatesDuringHistoryDrain = HOME_CFG.historyDrain.suppressChartUpdatesDuringDrain;
  // Debug/test: allow toggling spike-guard for hashrate series (default: enabled)
  private enableHashrateSpikeGuard: boolean = HOME_CFG.graphGuard.enableHashrateSpikeGuard;
  public debugSpikeGuard: boolean = false;
  private readonly graphGuardEngine = new GraphGuard({
    confirmSamples: this.graphGuardConfirmSamples,
    liveRefTolerance: this.graphGuardLiveRefTolerance,
    bigStepRel: this.graphGuardBigStepRel,
    liveRefStableSamples: this.graphGuardLiveRefStableSamples,
    liveRefStableRel: this.graphGuardLiveRefStableRel,
    debug: this.debugSpikeGuard,
  });
  public debugPillsLayout: boolean = false;
  // Adaptive axis padding so lines don't stick to frame; tweak here.
  private axisPadCfg = createAxisPaddingCfg();

  // --- Warmup / restart gating (controlled start sequence after miner restarts)
  private readonly warmupMachine = new HomeWarmupMachine(HOME_CFG.warmup);
  private warmupStagePrev: string = this.warmupMachine.getStage();

  // --- Startup behavior for hashrate (GraphGuard bypass for initial points)
  private expectedHashrateHsLast: number = 0;
  private startupUnlocked: boolean = false;
  private bypassRemaining: Record<string, number> = {};
  // 1m hashrate plotting must not start from 0 after restart; gate the very first plotted point.
  private hr1mStarted: boolean = false;
  // Timestamp (ms) when the first visible 1m hashrate point was plotted after a restart.
  // Used for "super smooth" startup: temporarily require more confirmation before accepting
  // short-lived dips. After the window, we switch to a snappier confirmation level.
  private hr1mStartTsMs: number | null = null;
  // Smooth startup should only trigger after an actual miner restart (hard cut), not on normal page loads.
  private hr1mSmoothArmed: boolean = false;
  private hr1mRestartTokenMs: number | null = null;
  private hr1mReloadTimer: any = null;
  private readonly hr1mReloadConsumedKey: string = '__nerdCharts_hr1mReloadConsumedToken';
  private readonly hr1mReloadCooldownUntilKey: string = '__nerdCharts_hr1mReloadCooldownUntil';
  private isHistoryImporting: boolean = false;
  // NOTE: For hashrate charts, the pill/live value is used ONLY as a warmup gate signal.
  // The plotted data continues to come from the history series (as before).
  // To avoid a visible "shoot" or a brief drop right after restart, we simply do NOT start
  // plotting 1m until the HISTORY 1m value itself is valid and has reached the expected unlock ratio.

  private debugAxisPadding: boolean = false;
  private readonly axisPadOverrideEnabledKey: string = '__nerdCharts_axisPaddingOverrideEnabled';
  private readonly axisPadStorageKey: string = '__nerdCharts_axisPadding';
  public nerdOsLogoColor: string = hexToRgba(HOME_CFG.colors.hashrateBase, 0.6);

  ngAfterViewChecked(): void {
    // Ensure chart is initialized only once when the canvas becomes available
    if (!this.chartInitialized && this.ctx && this.ctx.nativeElement) {
      this.chartInitialized = true; // Prevent re-initialization
      this.initChart();
    }

    // Keep the Input Current bar coloring in sync even when it hits 100%
    this.barDomSync.syncCurrentInputBarMaxFill(!!this.currentInputBarMaxWanted, this.currentThemeName);

    // Keep the VR Temp bar coloring in sync for the CRIT band (>= 99%)
    this.barDomSync.syncVrTempBarCritFill(!!this.vrTempBarCritWanted, this.currentThemeName);
  }

  private initChart(): void {
    this.chart = createHomeChart(this.ctx.nativeElement, this.chartData, this.chartOptions);
    // Restore legend visibility
    const storedVisibility = this.chartStorage.loadLegendVisibility();
    const visibility = storedVisibility ?? [
      !!HOME_CFG.uiDefaults.legendHidden.hr1m,
      !!HOME_CFG.uiDefaults.legendHidden.hr10m,
      !!HOME_CFG.uiDefaults.legendHidden.hr1h,
      !!HOME_CFG.uiDefaults.legendHidden.hr1d,
      !!HOME_CFG.uiDefaults.legendHidden.vregTemp,
      !!HOME_CFG.uiDefaults.legendHidden.asicTemp,
    ];

    if (visibility && visibility.length) {
      visibility.forEach((hidden: boolean, i: number) => {
        if (hidden) this.chart.getDatasetMeta(i).hidden = true;
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
    private localStorage: LocalStorageService,
    private hostEl: ElementRef<HTMLElement>,
    private renderer: Renderer2
  ) {
    // Local persistence wrapper for chart state/settings
    this.chartStorage = new HomeChartStorage({
      getItem: (k) => this.localStorageGet(k),
      setItem: (k, v) => this.localStorageSet(k, v),
      removeItem: (k) => this.localStorageRemove(k),
    });

    // Restore chart collapsed state (visual-only)
    this.isChartCollapsed = this.localStorageGet(this.chartCollapsedKey) === '1';

    this.barDomSync = new HomeBarDomSync(this.hostEl, this.renderer, HOME_CFG.tiles.domSync);

    this.historyDrainer = new HomeHistoryDrainer(
      {
        fetchInfo: (startTimestampMs, chunkSize) =>
          this.systemService.getInfoWithSpan(startTimestampMs, chunkSize, HOME_CFG.xAxis.maxWindowMs),
        importHistoryChunk: (history) => this.importHistoricalData(history),
        setRunning: (running) => (this.historyDrainRunning = running),
        setSuppressed: (suppressed) => (this.suppressChartUpdatesDuringHistoryDrain = suppressed),
        render: () => this.updateChart(),
        finalize: () => {
          this.filterOldData();
          if (this.wasLoaded) {
            this.saveChartData();
          }
        },
        log: (...args: any[]) => {
          // keep noise low unless debug flags are enabled
          if (this.debugSpikeGuard || this.debugAxisPadding) {
            // eslint-disable-next-line no-console
            console.log(...args);
          }
        },
      },
      {
        chunkSize: this.chunkSizeDrainer,
        renderThrottleMs: this.historyDrainRenderThrottleMs,
        useThrottledRender: this.historyDrainUseThrottledRender,
      }
    );
    // Load optional min-history timestamp (used after debug clear to prevent immediate refill)
    try {
      const v = Number(this.chartStorage.loadMinHistoryTimestampMs());
      if (Number.isFinite(v) && v > 0) {
        this.historyMinTimestampMs = v;
      }
    } catch {}

    const cfg = createHomeChartConfig({
      series: {
        labels: this.dataLabel,
        hr1m: this.dataData1m,
        hr10m: this.dataData10m,
        hr1h: this.dataData1h,
        hr1d: this.dataData1d,
        vregTemp: this.dataVregTemp,
        asicTemp: this.dataAsicTemp,
      },
      translate: (key: string) => this.translateService.instant(key),
      maxTicksLimit: this.hashrateYAxisMaxTicks,
      getTimeFormatIs12h: () => this.localStorage.getItem('timeFormat') === '12h',
      formatHashrate: (v: number) => HashSuffixPipe.transform(v),
      persistLegendVisibility: (visibility: boolean[]) => this.chartStorage.saveLegendVisibility(visibility),
      debugPillsLayout: this.debugPillsLayout,
    });

    this.chartData = cfg.chartData;
    this.chartOptions = cfg.chartOptions;
    applyHomeChartTheme(this.chartOptions);

    this.info$ = createSystemInfoPolling$({
      pollMs: 5000,
      chunkSize: this.chunkSizeDrainer,
      historyWindowMs: HOME_CFG.xAxis.maxWindowMs,
      fetchInfo: (startTimestampMs, chunkSize) =>
        this.systemService.getInfoWithSpan(startTimestampMs, chunkSize, HOME_CFG.xAxis.maxWindowMs),
      defaultInfo: () => SystemService.defaultInfo(),
      getStoredLastTimestampMs: () => this.getStoredTimestamp(),
      getForceStartTimestampMs: () => {
        try {
          const forcedStart = Number(this.localStorageGet('__nerdCharts_forceStartTimestampMs'));
          return Number.isFinite(forcedStart) && forcedStart > 0 ? forcedStart : null;
        } catch {
          return null;
        }
      },
      clearForceStartTimestampMs: () => {
        try {
          this.localStorageRemove('__nerdCharts_forceStartTimestampMs');
        } catch {}
      },
      logError: (...args: any[]) => console.error(...args),
      onInfo: (info) => {
        if (!info) return;
        this._info = info;
        try {
          const flagKey = '__nerdCharts_clearChartHistoryOnce';
          if (this.localStorageGet(flagKey) === '1') {
            this.localStorageRemove(flagKey);
            this.clearChartHistoryInternal(false);
          }
        } catch {}

        // Skip chart updates until the chart is actually created
        if (!this.chart) {
          return;
        }

        // --- Warmup / startup signals (use live values, not history series)
        // expectedHashRate$ returns an "expected" value used in UI. For internal comparisons
        // we keep everything in H/s to match live pool sums and chart values.
        try {
          const expectedGh = Math.floor(Number(info.frequency) * ((Number(info.smallCoreCount) * Number(info.asicCount)) / 1000));
          const expectedHs = Number.isFinite(expectedGh) && expectedGh > 0 ? expectedGh * 1e9 : 0;
          this.expectedHashrateHsLast = expectedHs;
        } catch {
          this.expectedHashrateHsLast = 0;
        }

        const nowMs = Date.now();
        const liveHs = this.getPoolHashrateHsSum();
        const unlockRatio = Number(HOME_CFG.startup.expectedUnlockRatio ?? 0.75);
        // 1m hashrate warmup gate: only unlock once expected is known and live reaches
        // the configured ratio (e.g. 75%). If expected is 0/unknown, keep locked.
        const unlockOk = this.expectedHashrateHsLast > 0
          ? (Number.isFinite(liveHs) && liveHs >= this.expectedHashrateHsLast * unlockRatio)
          : false;

        // "systemOk" is a cheap proxy to detect restarts even if temperatures remain high.
        // After a real restart, expectedHashrate/frequency often drops to 0 or becomes unstable for a moment.
        const systemOk = Number.isFinite(this.expectedHashrateHsLast) && this.expectedHashrateHsLast > 0;
        this.warmupMachine.observeLive({
          nowMs,
          vregTempC: (info as any).vrTemp,
          asicTempC: (info as any).temp,
          liveHashrateHs: liveHs,
          expectedHashrateHs: this.expectedHashrateHsLast,
          systemOk,
          unlockOk,
        });

        // Track stage changes (useful for debug, but also allows future hooks).
        this.warmupStagePrev = this.warmupMachine.getStage();

        // Only drain on cold start (no cached points yet)
        if (this.dataLabel.length === 0) {
          this.importHistoricalDataChunked(info.history);
        } else {
          this.importHistoricalData(info.history);
        }
      },
      mapInfo: (info) => {
        // Normalize/derive everything the tiles need (bars + squares).
        const derived = normalizeHomeTileInfo(info as any, {
          powerUsageAliases: HOME_CFG.tiles.powerUsageAliases,
          vrTempLimits: (BAR_LIMITS as any).vrTemp,
        });

        this.currentInputBarMaxWanted = derived.currentInputBarMaxWanted;
        this.vrTempBarCritWanted = derived.vrTempBarCritWanted;

        this.isDualPool = derived.isDualPool;
        this.hasChipTemps = derived.hasChipTemps;

        return info;
      },
    });

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

  public toggleChartCollapsed(evt?: Event): void {
    this.isChartCollapsed = !this.isChartCollapsed;

    // Remove focus after click so Nebular doesn't keep the button in a "pressed"/focused visual state.
    // (Keeps the interaction clean while still allowing keyboard users to focus intentionally.)
    try { (evt?.currentTarget as HTMLElement | null)?.blur?.(); } catch {}

    if (this.isChartCollapsed) {
      this.localStorageSet(this.chartCollapsedKey, '1');
      return;
    }

    // Expanded again: clear persisted flag and make sure Chart.js recalculates layout.
    this.localStorageRemove(this.chartCollapsedKey);
    setTimeout(() => {
      try { (this.chart as any)?.resize?.(); } catch {}
      try { this.chart?.update?.('none' as any); } catch {}
    }, 280);
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
   * Ensure the "Input current" meter bar can still colorize correctly.
   *
   * The HTML expects these aliases:
   *  - info.currentA
   *  - info.minCurrentA
   *  - info.maxCurrentA
   *
   * Priority for limits:
   *  1) If the backend already provides explicit current limits (in A or mA), keep them.
   *  2) Otherwise derive maxCurrentA from configured power/voltage bounds.
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
    this.chartWindowMs = clampWindowMs(HOME_CFG.xAxis.fixedWindowMs, this.zoomCfg);
    // Chart.js plugins are global; register once.
    registerHomeChartPlugins();
    installNerdChartsDebugBootstrap(globalThis, {
      storage: {
        getItem: (k: string) => this.localStorageGet(k),
        setItem: (k: string, v: string) => this.localStorageSet(k, v),
        removeItem: (k: string) => this.localStorageRemove(k),
      },
      clearChartHistoryInternal: (updateChartNow: boolean) => this.clearChartHistoryInternal(!!updateChartNow),
      setAxisPadding: (cfg: any, persist: boolean) => this.setAxisPadding(cfg, persist),
      saveAxisPaddingOverrides: () => this.saveAxisPaddingOverrides(),
      disableAxisPaddingOverride: () => {
        try { window?.localStorage?.removeItem(this.axisPadOverrideEnabledKey); } catch (e) {
          console.warn("[nerdCharts] axis padding override disable failed", e);
        }
      },
      setHashrateTicks: (n: number) => this.setHashrateYAxisLabelCount(n),
      setHashrateMinTickStep: (ths: number) => {
        this.hashrateYAxisMinStepThs = ths;
        this.updateAxesScaleAdaptive();
        try { this.chart?.update?.("none"); } catch { try { this.chart?.update?.(); } catch {} }
      },
      dumpAxisScale: () => {
        try {
          const y: any = (this.chartOptions.scales as any).y || {};
          const ticks: any = y.ticks || {};
          return {
            yMin: y.min,
            yMax: y.max,
            maxTicksLimit: ticks.maxTicksLimit,
            stepSize: ticks.stepSize,
            hashrateYAxisMaxTicks: this.hashrateYAxisMaxTicks,
            hashrateYAxisMinStepThs: this.hashrateYAxisMinStepThs,
          };
        } catch (e: any) {
          return { error: String(e) };
        }
      },
      flushHistoryDrainRender: () => {
        try {
          this.filterOldData();
          if (this.wasLoaded) this.saveChartData();
          this.updateChart();
        } catch {}
      },

      // Console helper: restart device via backend endpoint.
      // Note: mirrors the SystemComponent.restart() backend call; OTP is optional depending on device settings.
      restart: async (totp?: string) => {
        try {
          const res = await firstValueFrom(this.systemService.restart('', (totp || '').trim()));
          return { ok: true, res };
        } catch (e: any) {
          // eslint-disable-next-line no-console
          console.warn('[nerdCharts] restart failed', e);
          return { ok: false, error: String(e) };
        }
      },
    });
    this.graphGuardEngine.configure({ debug: !!this.debugSpikeGuard });
    this.loadAxisPaddingOverrides();
    this.themeSubscription = this.themeService.getJsTheme().subscribe((t: any) => {
      // Nebular emits the current theme object here (includes a `name`).
      this.currentThemeName = String(t?.name ?? '').trim();
      this.updateThemeColors();
      // Re-apply any bar overrides that are theme-dependent.
      this.barDomSync.syncCurrentInputBarMaxFill(!!this.currentInputBarMaxWanted, this.currentThemeName);
      this.barDomSync.syncVrTempBarCritFill(!!this.vrTempBarCritWanted, this.currentThemeName);
    });

    // Listen for timeFormat changes
    this.timeFormatListener = () => {
      this.updateTimeFormat();
    };
    window.addEventListener('timeFormatChanged', this.timeFormatListener);

    // Pull `/asic` once so the Current Frequency bar can use the same
    // model-specific frequency table as the Settings screen.
    (async () => {
      try {
        const asic = await firstValueFrom(this.systemService.getAsicInfo(''));
        this._asicInfo = asic;
        this._freqBoundsCacheKey = null;
        this._voltBoundsCacheKey = null;
      } catch {
        // ignore
      }
    })();
}

  ngOnDestroy(): void {
    // Destroy Chart.js instance to release canvas resources and internal listeners.
    if (this.chart) {
      this.chart.destroy();
      this.chart = undefined;
    }

    // Clean up theme subscription to avoid memory leaks when navigating away.
    this.themeSubscription?.unsubscribe();
    this.historyDrainer?.stop();
    // Cancel any pending auto-reload timer.
    this.clearHr1mReloadTimer();
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
    this.isHistoryImporting = true;
    try {
      this.updateChartData(data);
    } finally {
      this.isHistoryImporting = false;
    }

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
    this.chartState.clear();
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
        this.graphGuardEngine.observeLiveRef(hs);
      }
      return hs;
    } catch {
      return 0;
    }
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

    // Always enforce a stable X-window (e.g. 1h), regardless of how many points exist.
    const { xMinMs, xMaxMs } = computeXWindow(this.dataLabel || [], this.chartWindowMs);
    this.applyXWindowToChart(xMinMs, xMaxMs);

    const labels = this.dataLabel || [];

    // If there are no labels yet, we still keep the fixed X-window (handled above),
    // but we can't compute Y-bounds without data.
    if (!labels.length) return;

    const temp4 = this.chart.isDatasetVisible(4);
    const temp5 = this.chart.isDatasetVisible(5);
    const vreg = temp4 ? this.dataVregTemp : (!temp4 && !temp5 ? this.dataVregTemp : null);
    const asic = temp5 ? this.dataAsicTemp : (!temp4 && !temp5 ? this.dataAsicTemp : null);

    const visibility = {
      hr1m: this.chart.isDatasetVisible(0),
      hr10m: this.chart.isDatasetVisible(1),
      hr1h: this.chart.isDatasetVisible(2),
      hr1d: this.chart.isDatasetVisible(3),
    };
    const series = {
      hr1m: this.dataData1m,
      hr10m: this.dataData10m,
      hr1h: this.dataData1h,
      hr1d: this.dataData1d,
      vregTemp: vreg ?? [],
      asicTemp: asic ?? [],
    };

    const { bounds, tempAxisMin, tempAxisMax } = computeHomeChartScales({
      labels,
      xMinMs,
      xMaxMs,
      series,
      visibility,
      axisPadCfg: this.axisPadCfg,
      maxTicks: this.hashrateYAxisMaxTicks,
      hashrateMinStepThs: this.hashrateYAxisMinStepThs,
      tempMinStepC: this.tempYAxisMinStepC,
      liveRefHs: this.lastLivePoolSumHs,
      softIncludeRel: HOME_CFG.yAxis.hashrateSoftIncludeRel,
      axisMinPadC: HOME_CFG.tempScale.axisMinPadC,
      axisMaxPadC: HOME_CFG.tempScale.axisMaxPadC,
      tempHysteresisC: HOME_CFG.tempScale.hysteresisC,
      prevTempMin: this.lastTempAxisMin,
      prevTempMax: this.lastTempAxisMax,
    });

    if (Number.isFinite(tempAxisMin as any)) this.lastTempAxisMin = tempAxisMin as number;
    if (Number.isFinite(tempAxisMax as any)) this.lastTempAxisMax = tempAxisMax as number;

    applyAxisBoundsToChartOptions(this.chartOptions, bounds);

  }

  // --- Sanitizing helpers (invalid samples become NaN => visual gap / never plotted)

  private sanitizeHashrateHs(v: any): number {
    const n = Number(v);
    if (!Number.isFinite(n)) return NaN;
    if (n < HOME_CFG.sanitize.hashrateMinHs) return NaN;
    return n;
  }

  private sanitizeTempC(v: any): number {
    const n = Number(v);
    if (!Number.isFinite(n)) return NaN;
    // Never plot negative temps or crazy sensor values. Warmup gating is handled separately.
    if (n < HOME_CFG.sanitize.tempMinC) return NaN;
    if (n > HOME_CFG.sanitize.tempMaxC) return NaN;
    return n;
  }

  // --- Optional: one-time page reload after smooth 1m startup (only after miner restart)

  private clearHr1mReloadTimer(): void {
    if (this.hr1mReloadTimer != null) {
      try { clearTimeout(this.hr1mReloadTimer); } catch {}
      this.hr1mReloadTimer = null;
    }
  }

  private scheduleHr1mReloadAfterSmooth(): void {
    if (!HOME_CFG.startup.hr1mReloadAfterSmooth) return;
    if (!this.hr1mSmoothArmed) return;
    if (this.hr1mRestartTokenMs == null) return;

    const windowMs = Math.max(0, Math.round(Number(HOME_CFG.startup.hr1mSmoothWindowMs ?? 0)));
    if (!windowMs) return;

    // Guard: only once per restart token (session-scoped) + cooldown to avoid loops.
    const token = String(this.hr1mRestartTokenMs);
    try {
      const consumed = sessionStorage.getItem(this.hr1mReloadConsumedKey);
      if (consumed === token) {
        this.hr1mSmoothArmed = false;
        return;
      }

      const now = Date.now();
      const cooldownUntil = Number(sessionStorage.getItem(this.hr1mReloadCooldownUntilKey) ?? 0);
      if (Number.isFinite(cooldownUntil) && cooldownUntil > now) {
        this.hr1mSmoothArmed = false;
        return;
      }

      sessionStorage.setItem(this.hr1mReloadConsumedKey, token);
      const cooldownMs = Math.max(0, Math.round(Number(HOME_CFG.startup.hr1mReloadCooldownMs ?? 0)));
      if (cooldownMs) {
        sessionStorage.setItem(this.hr1mReloadCooldownUntilKey, String(now + cooldownMs));
      }
    } catch {
      // If sessionStorage is not available, still try to reload (best effort).
    }

    this.clearHr1mReloadTimer();
    this.hr1mSmoothArmed = false; // ensure we don't schedule twice for the same restart
    this.hr1mReloadTimer = setTimeout(() => {
      try {
        window.location.reload();
      } catch {
        try { (location as any).reload(); } catch {}
      }
    }, windowMs);
  }

  /**
   * Inserts a single NaN break-point across all series to create a hard visual cut.
   * This is used on restarts so charts don't "fall" to 0 but end cleanly.
   */
  private insertHardBreakSample(breakAtMs: number): void {
    // Reset startup/bypass state per restart.
    this.startupUnlocked = false;
    this.bypassRemaining = {};
    this.hr1mStarted = false;
    this.hr1mStartTsMs = null;

    // Cancel any pending auto-reload from a previous restart cycle.
    this.clearHr1mReloadTimer();

    // Arm smooth-start behavior ONLY after an actual restart/hard cut.
    // This prevents smooth mode (and optional reload) from triggering on normal page loads/refreshes.
    this.hr1mSmoothArmed = true;

    // Start a fresh guard baseline for the new post-restart segment.
    // (Otherwise the first post-restart point may be compared against stale pre-restart state.)
    try { this.graphGuardEngine.reset(); } catch {}

    const last = this.dataLabel.length ? this.dataLabel[this.dataLabel.length - 1] : 0;
    const ts = Math.max(Number(breakAtMs) || Date.now(), last > 0 ? last + 1 : 0);
    if (last > 0 && ts <= last) return;

    // Remember the break timestamp to prevent a later in-place overwrite when the next
    // history point happens to have the same timestamp (would remove the visual gap).
    this.lastHardBreakTs = ts;

    // Use the break timestamp as a per-restart token for "run once" logic.
    this.hr1mRestartTokenMs = ts;

    this.dataLabel.push(ts);
    this.dataData1m.push(NaN);
    this.dataData10m.push(NaN);
    this.dataData1h.push(NaN);
    this.dataData1d.push(NaN);
    this.dataVregTemp.push(NaN);
    this.dataAsicTemp.push(NaN);

    // Advance stored timestamp so polling doesn't keep refetching the same restart window.
    this.storeTimestamp(ts);
  }

  private ensureStartupUnlocked(livePoolSumHs: number): void {
    if (this.startupUnlocked) return;

    const live = Number(livePoolSumHs);
    const expected = Number(this.expectedHashrateHsLast);
    const ratio = Number(HOME_CFG.startup.expectedUnlockRatio ?? 0.75);
    const liveStable = !!this.graphGuardEngine?.isLiveRefStable?.();

    if (shouldUnlockStartup({
      liveHs: live,
      expectedHs: expected,
      expectedUnlockRatio: ratio,
      liveIsStable: liveStable,
    })) {
      this.startupUnlocked = true;
      const n = Math.max(0, Math.round(Number(HOME_CFG.startup.bypassGuardSamples ?? 0)));
      this.bypassRemaining = {
        // Do NOT bypass GraphGuard for 1m hashrate.
        // The 1m history stream can emit a short transient low plateau shortly after restart
        // (typically 2–3 ticks) even though the miner is already hashing steadily.
        // Bypassing the guard for 1m would let that dip through and create the visible "Zack".
        // We keep the optional bypass for the slower series only.
        hashrate_1m: 0,
        hashrate_10m: n,
        hashrate_1h: n,
        hashrate_1d: n,
      };
    }
  }

  private shouldBypassHashGuard(key: string): boolean {
    if (!this.startupUnlocked) return false;
    const left = Math.max(0, Math.round(Number(this.bypassRemaining?.[key] ?? 0)));
    return left > 0;
  }

  private consumeBypassHashGuard(key: string): void {
    if (!this.startupUnlocked) return;
    const left = Math.max(0, Math.round(Number(this.bypassRemaining?.[key] ?? 0)));
    if (left <= 0) return;
    this.bypassRemaining[key] = left - 1;
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
    this.graphGuardEngine.configure({ debug: this.debugSpikeGuard });

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

    // If a restart was detected (warmup reset), insert a single NaN "break" sample
    // to end all curves cleanly. We place it just before the next incoming history point
    // so it can never be overwritten by a duplicate timestamp.
    if (this.warmupMachine.consumeBreakPending()) {
      const nextTs = newData.length ? Number(newData[0].timestamp) : Date.now();
      const breakAt = Number.isFinite(nextTs) ? (nextTs - 1) : Date.now();
      this.insertHardBreakSample(breakAt);
    }

    // Append only new data (spike-guarded, line-break free)
    // If we push duplicates, Chart.js draws a vertical segment at the same X ("treppenhaus").
    for (const entry of newData) {
      // If we inserted a hard NaN break-point, ensure the next incoming history sample
      // cannot overwrite it via the duplicate-timestamp in-place update path.
      if (this.lastHardBreakTs && Number(entry.timestamp) <= this.lastHardBreakTs) {
        entry.timestamp = this.lastHardBreakTs + 1;
      }
      // Warmup gates (progress is driven by live values from polling)
      const vregEnabled = this.warmupMachine.isVregEnabled();
      const asicEnabled = this.warmupMachine.isAsicEnabled();
      const hr1mEnabled = this.warmupMachine.isHr1mEnabled();
      const otherHashEnabled = this.warmupMachine.isOtherHashEnabled();

      // Startup unlock for optional GraphGuard bypass (uses hashrate pill / live pool sum)
      this.ensureStartupUnlocked(livePoolSum);

      // Sanitize raw inputs (invalid => NaN => never plotted)
      const hr1mRaw = this.sanitizeHashrateHs(entry.hashrate_1m);
      const hr10mRaw = this.sanitizeHashrateHs(entry.hashrate_10m);
      const hr1hRaw = this.sanitizeHashrateHs(entry.hashrate_1h);
      const hr1dRaw = this.sanitizeHashrateHs(entry.hashrate_1d);
      const vregRaw = this.sanitizeTempC(entry.vregTemp);
      const asicRaw = this.sanitizeTempC(entry.asicTemp);

      // 1m hashrate: avoid starting from 0/low history samples.
      // When warmup enables 1m, we seed the first visible point from the live pill.
      // Afterwards, if the history 1m value is still bogus (<=0 / NaN) but the pill is live,
      // we keep plotting the live pill as a proxy to prevent brief drops.
            // 1m hashrate start gating:
      // - Pill/live is ONLY used to decide when we're allowed to start (startupUnlocked).
      // - The plotted value still comes from the history (hr1mRaw), as before.
      // - To avoid any visible "shoot" from 0 or a short drop, we only start once the HISTORY 1m
      //   itself is valid and has reached the expected unlock ratio as well.
      let hr1mCandidate: number = NaN;
      if (hr1mEnabled) {
        const expectedHs = Number(this.expectedHashrateHsLast);
        const ratio = Number(HOME_CFG.startup.expectedUnlockRatio ?? 0.75);
        const histOk = Number.isFinite(hr1mRaw) && hr1mRaw > 0;

        // Require the history value to also be at/above the unlock ratio on first start.
        // After start, we keep using the history value (GraphGuard will smooth rare glitches).
        const histUnlockOk = (expectedHs > 0)
          ? (histOk && hr1mRaw >= expectedHs * ratio)
          : histOk;

        if (!this.hr1mStarted) {
          if (shouldStartHr1mFromHistory({
            hr1mStarted: this.hr1mStarted,
            startupUnlocked: this.startupUnlocked,
            histOk,
            histUnlockOk,
            isHistoryImporting: this.isHistoryImporting,
          })) {
            this.hr1mStarted = true;
            // Smooth startup + optional reload should only trigger after an actual restart (hard cut).
            // On normal page loads, we keep snappy behavior (no smooth window).
            if (this.hr1mSmoothArmed) {
              this.hr1mStartTsMs = Number(entry.timestamp);
              this.scheduleHr1mReloadAfterSmooth();
            } else {
              this.hr1mStartTsMs = null;
            }
            hr1mCandidate = hr1mRaw;
          } else {
            hr1mCandidate = NaN;
          }
        } else {
          hr1mCandidate = hr1mRaw;
        }
      }
      // --- Restart hard-cut detection based on incoming history samples.
      // We intentionally do NOT rely on temperatures dropping quickly.
      // If live hashrate (pill) is gone and the history starts emitting boot/glitch samples
      // (0/NaN temps or 0 hashrate), we cut immediately so the curve doesn't fall to the 0-line.
      const liveOkNow = Number.isFinite(livePoolSum) && livePoolSum > 0;
      const stageNow = this.warmupMachine.getStage();
      const historyHr1m = Number(entry.hashrate_1m);
      const restartMarker = shouldInsertRestartCut({
        liveOkNow,
        vregRaw,
        asicRaw,
        historyHr1m,
        tempMinValidC: Number(HOME_CFG.warmup.tempMinValidC ?? 10),
      });

      if (stageNow === 'READY' && restartMarker) {
        this.warmupMachine.reset(entry.timestamp);
        // Consume the break flag immediately and insert the cut at the current timestamp.
        this.warmupMachine.consumeBreakPending();
        // Place the break just before the first post-restart timestamp so the cut
        // can't be overwritten by an in-place update at the same X.
        this.insertHardBreakSample(entry.timestamp - 1);
        // Do not append this (bogus) sample; also advance the stored timestamp.
        this.storeTimestamp(entry.timestamp);
        continue;
      }

      // While locked (restart window / VR delay), do not append any new points (no tracking).
      // Curves were already terminated via the break marker.
      if (this.warmupMachine.isLocked()) {
        this.storeTimestamp(entry.timestamp);
        continue;
      }

      const applyHash = (key: string, v: number, thr: number): number => {
        if (!Number.isFinite(v)) return NaN;
        // Optional startup bypass for GraphGuard: allow the first N samples after warmup
        // to be plotted unguarded (prevents brief artificial drops caused by an unstable
        // live reference right after restart). This does NOT trigger on frequency changes
        // because startupUnlocked is only set once when live reaches the expected ratio.
        if (this.shouldBypassHashGuard(key)) {
          this.consumeBypassHashGuard(key);
          return v;
        }
        // Super smooth startup for 1m: for ~2 minutes after restart-start, require more
        // confirmation to accept short-lived dips (prevents 2-3 tick artifacts). After
        // that, switch to a snappier confirmation level.
        let confirmOverride: number | undefined = undefined;
        if (key === 'hashrate_1m') {
          if (this.hr1mStarted) {
            const start = this.hr1mStartTsMs;
            const inStartupWindow = start != null && (Number(entry.timestamp) - start) <= HOME_CFG.startup.hr1mSmoothWindowMs;
            confirmOverride = inStartupWindow ? HOME_CFG.startup.hr1mConfirmStartup : HOME_CFG.startup.hr1mConfirmNormal;
          } else {
            // If not started yet (or restored from storage), default to snappy.
            confirmOverride = HOME_CFG.startup.hr1mConfirmNormal;
          }
        }

        return this.enableHashrateSpikeGuard
          ? this.graphGuardEngine.apply(key, v, thr, livePoolSum, confirmOverride)
          : v;
      };

      const applyTemp = (key: string, v: number, thr: number): number => {
        if (!Number.isFinite(v)) return NaN;
        return this.graphGuardEngine.apply(key, v, thr);
      };

      const lastIdx = this.dataLabel.length - 1;
      if (lastIdx >= 0 && this.dataLabel[lastIdx] === entry.timestamp) {
        // In-place update for duplicate timestamp (keep gating + sanitizing)
        this.dataVregTemp[lastIdx] = vregEnabled ? applyTemp('vregTemp', vregRaw, HOME_CFG.graphGuard.thresholds.vregTemp) : NaN;
        this.dataAsicTemp[lastIdx] = asicEnabled ? applyTemp('asicTemp', asicRaw, HOME_CFG.graphGuard.thresholds.asicTemp) : NaN;

        const hr1mOut = hr1mEnabled
          ? applyHash('hashrate_1m', hr1mCandidate, HOME_CFG.graphGuard.thresholds.hashrate1m)
          : NaN;
        this.dataData1m[lastIdx] = hr1mOut;
        if (hr1mEnabled && Number.isFinite(hr1mOut)) {
          this.warmupMachine.notifyHr1mFlow(entry.timestamp);
        }

        this.dataData10m[lastIdx] = otherHashEnabled ? applyHash('hashrate_10m', hr10mRaw, HOME_CFG.graphGuard.thresholds.hashrate10m) : NaN;
        this.dataData1h[lastIdx] = otherHashEnabled ? applyHash('hashrate_1h', hr1hRaw, HOME_CFG.graphGuard.thresholds.hashrate1h) : NaN;
        this.dataData1d[lastIdx] = otherHashEnabled ? applyHash('hashrate_1d', hr1dRaw, HOME_CFG.graphGuard.thresholds.hashrate1d) : NaN;
        continue;
      }

      this.dataLabel.push(entry.timestamp);

      this.dataVregTemp.push(vregEnabled ? applyTemp('vregTemp', vregRaw, HOME_CFG.graphGuard.thresholds.vregTemp) : NaN);
      this.dataAsicTemp.push(asicEnabled ? applyTemp('asicTemp', asicRaw, HOME_CFG.graphGuard.thresholds.asicTemp) : NaN);

      const hr1mOut = hr1mEnabled
        ? applyHash('hashrate_1m', hr1mCandidate, HOME_CFG.graphGuard.thresholds.hashrate1m)
        : NaN;
      this.dataData1m.push(hr1mOut);
      if (hr1mEnabled && Number.isFinite(hr1mOut)) {
        this.warmupMachine.notifyHr1mFlow(entry.timestamp);
      }

      this.dataData10m.push(otherHashEnabled ? applyHash('hashrate_10m', hr10mRaw, HOME_CFG.graphGuard.thresholds.hashrate10m) : NaN);
      this.dataData1h.push(otherHashEnabled ? applyHash('hashrate_1h', hr1hRaw, HOME_CFG.graphGuard.thresholds.hashrate1h) : NaN);
      this.dataData1d.push(otherHashEnabled ? applyHash('hashrate_1d', hr1dRaw, HOME_CFG.graphGuard.thresholds.hashrate1d) : NaN);
    }
  }

  private loadChartData(): void {
    // Allow persistence from now on (even if there is no data yet on first run).
    this.wasLoaded = true;

    const persisted = this.chartStorage.loadPersistedState();
    if (!persisted) return;

    try {
      this.chartState = HomeChartState.fromPersisted(persisted);

      // IMPORTANT: persisted history can contain bogus 0/invalid samples (e.g. during restarts)
      // that must never be plotted. During a normal run, warmup gating prevents these samples
      // from being pushed, but on a page refresh we restore raw arrays and must sanitize them
      // again so refreshes never re-introduce spikes to the 0-line.
      this.sanitizeLoadedHistory();

      // If we already have finite 1m points (from persisted history), treat startup as already started.
      try {
        const lastFinite = findLastFinite(this.dataData1m);
        this.hr1mStarted = Number.isFinite(lastFinite as any);
      } catch {
        this.hr1mStarted = false;
      }

      // Keep chartData in sync with the restored arrays.
      if (this.chartData) {
        this.updateChart();
      }

      this.maybeExpandHistoryForZoom();
    } catch (err) {
      console.warn('[HomeComponent] Failed to load chartData from storage (keeping it untouched).', err);

      // Reset in-memory only, but do NOT wipe storage automatically.
      this.chartState.clear();

      if (this.chartData) {
        this.updateChart();
      }
    }
  }

  private maybeExpandHistoryForZoom(): void {
    const maxWindow = this.zoomCfg?.maxWindowMs ?? 0;
    if (!maxWindow || !this.dataLabel?.length) return;

    const oldest = this.dataLabel[0];
    const cutoff = Date.now() - maxWindow;
    if (!Number.isFinite(oldest as any) || Number(oldest) > cutoff + 2000) {
      void this.reloadHistoryForWindow(maxWindow);
    }
  }

  private sanitizeLoadedHistory(): void {
    const len = this.dataLabel.length;
    if (!len) return;

    // Re-run loaded points through the spike-guard so cached spikes can't persist.
    this.graphGuardEngine.reset();

    const out1m: number[] = [];
    const out10m: number[] = [];
    const out1h: number[] = [];
    const out1d: number[] = [];
    const outVreg: number[] = [];
    const outAsic: number[] = [];

    for (let i = 0; i < len; i++) {
      const hr1m = this.sanitizeHashrateHs(this.dataData1m[i]);
      const hr10m = this.sanitizeHashrateHs(this.dataData10m[i]);
      const hr1h = this.sanitizeHashrateHs(this.dataData1h[i]);
      const hr1d = this.sanitizeHashrateHs(this.dataData1d[i]);
      const vreg = this.sanitizeTempC(this.dataVregTemp[i]);
      const asic = this.sanitizeTempC(this.dataAsicTemp[i]);

      out1m.push(Number.isFinite(hr1m)
        ? (this.enableHashrateSpikeGuard
          ? this.graphGuardEngine.apply('hashrate_1m', hr1m, HOME_CFG.graphGuard.thresholds.hashrate1m)
          : hr1m)
        : NaN);

      out10m.push(Number.isFinite(hr10m)
        ? (this.enableHashrateSpikeGuard
          ? this.graphGuardEngine.apply('hashrate_10m', hr10m, HOME_CFG.graphGuard.thresholds.hashrate10m)
          : hr10m)
        : NaN);

      out1h.push(Number.isFinite(hr1h)
        ? (this.enableHashrateSpikeGuard
          ? this.graphGuardEngine.apply('hashrate_1h', hr1h, HOME_CFG.graphGuard.thresholds.hashrate1h)
          : hr1h)
        : NaN);

      out1d.push(Number.isFinite(hr1d)
        ? (this.enableHashrateSpikeGuard
          ? this.graphGuardEngine.apply('hashrate_1d', hr1d, HOME_CFG.graphGuard.thresholds.hashrate1d)
          : hr1d)
        : NaN);

      outVreg.push(Number.isFinite(vreg)
        ? this.graphGuardEngine.apply('vregTemp', vreg, HOME_CFG.graphGuard.thresholds.vregTemp)
        : NaN);

      outAsic.push(Number.isFinite(asic)
        ? this.graphGuardEngine.apply('asicTemp', asic, HOME_CFG.graphGuard.thresholds.asicTemp)
        : NaN);
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
    this.chartStorage.savePersistedState(this.chartState.toPersisted());
  }

  private filterOldData(): void {
    const now = new Date().getTime();
    // Keep the in-memory series consistent with the current x-axis viewport.
    this.chartState.trimToWindow(now, this.zoomCfg.maxWindowMs);

    if (this.chartState.labels.length) {
      this.storeTimestamp(this.chartState.labels[this.chartState.labels.length - 1]);
    }
  }

  private storeTimestamp(timestamp: number): void {
    if (this.saveLock) {
      return;
    }
    this.chartStorage.saveLastTimestamp(timestamp);
  }

  private getStoredTimestamp(): number | null {
    return this.chartStorage.loadLastTimestamp();
  }

  private syncChartDatasetsAndSmoothing(): void {
    syncHomeChartDataAndSmoothing({
      chartData: this.chartData,
      chartOptions: this.chartOptions,
      chart: this.chart,
      series: {
        labels: this.dataLabel,
        hr1m: this.dataData1m,
        hr10m: this.dataData10m,
        hr1h: this.dataData1h,
        hr1d: this.dataData1d,
        vregTemp: this.dataVregTemp,
        asicTemp: this.dataAsicTemp,
      },
      hashrateDisplayValue: this.getPoolHashrateHsSum(),
      smoothingCfg: this.hashrate1mSmoothingCfg,
      windowMs: this.chartWindowMs,
      zoomCfg: this.zoomCfg,
    });
  }

  private updateChart() {
    this.syncChartDatasetsAndSmoothing();

    if (!this.chart) {
      return;
    }

    this.updateAxesScaleAdaptive();
    this.chart.update();
  }

  private updateThemeColors(): void {
    applyHomeChartTheme(this.chartOptions);

    if (this.chart) {
      this.chart.options = this.chartOptions;
      this.chart.update();
    }
  }

  // Toggle only if feature exists, then persist

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

  private clearChartHistoryInternal(updateChartNow: boolean): void {
    this.historyDrainer?.stop();
    this.historyDrainRunning = false;

    this.clearChartData();
    this.graphGuardEngine.reset();

    // Clear persisted history
    this.chartStorage.clearPersistedState();
    this.chartStorage.clearLastTimestamp();

    // Prevent immediate refill with old history (API/local) after clearing.
    // Seed a minimum timestamp slightly in the past to allow the very next sample through.
    const seed = Date.now() - 30000;
    this.historyMinTimestampMs = seed;
    this.chartStorage.saveMinHistoryTimestampMs(seed);
    this.chartStorage.saveLastTimestamp(seed);

    if (updateChartNow) {
      this.updateChart();
    }
  }

  private async reloadHistoryForWindow(windowMs: number): Promise<void> {
    const start = Date.now() - Math.max(0, Math.round(Number(windowMs || 0)));
    let info: any = null;
    try {
      info = await firstValueFrom(
        this.systemService.getInfoWithSpan(start, this.chunkSizeDrainer, windowMs)
      );
    } catch {
      // ignore (next polling tick will retry)
      return;
    }

    if (!info?.history?.timestamps?.length) {
      return;
    }

    const existingOldest = this.dataLabel.length ? Number(this.dataLabel[0]) : null;
    const fetchedOldest = getHistoryOldestTimestampMs(info.history);
    const shouldReplace = !this.dataLabel.length
      || (Number.isFinite(existingOldest as any)
        && Number.isFinite(fetchedOldest as any)
        && (fetchedOldest as number) < (existingOldest as number) - 2000);

    // If the fetched history does NOT extend further back than what we already have,
    // avoid wiping local history (e.g. after a miner reboot when firmware history reset).
    if (!shouldReplace) {
      this.importHistoricalData(info.history);
      return;
    }

    // Reset state so older points can be re-imported in one pass.
    this.historyDrainer?.stop();
    this.historyDrainRunning = false;
    this.suppressChartUpdatesDuringHistoryDrain = false;

    this.clearChartData();
    this.graphGuardEngine.reset();

    this.chartStorage.clearPersistedState();
    this.chartStorage.clearLastTimestamp();
    this.chartStorage.clearMinHistoryTimestampMs();
    this.historyMinTimestampMs = null;

    this.importHistoricalData(info.history);
  }

  // edge case where chart data in the browser is not consistent
  // this happens when adding new charts

  public rejectRate(id?: number): number {
  // Template can call this before the first info payload arrived.
  // Be defensive to avoid breaking the whole dashboard render.
  const pools = this._info?.stratum?.pools;
  if (!Array.isArray(pools) || pools.length === 0) return 0;

  // In some template contexts (e.g. single pool tile) `idx` may be undefined.
  // For UI consistency we default to the PRIMARY pool (index 0), which also matches the Shares card.
  const idx = (typeof id === 'number' && Number.isFinite(id)) ? id : 0;

  const pool = pools[idx];
  if (!pool) return 0;

  const rejected = Number(pool.rejected ?? 0);
  const accepted = Number(pool.accepted ?? 0);

  const total = accepted + rejected;
  if (!total) return 0;

  return (rejected / total) * 100;
}
private importHistoricalDataChunked(history: any): void {
    this.historyDrainer.ingest(history);
  }
}
