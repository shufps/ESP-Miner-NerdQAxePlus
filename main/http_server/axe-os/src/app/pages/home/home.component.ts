import { Component, AfterViewChecked, OnInit, OnDestroy } from '@angular/core';
import { interval, map, Observable, shareReplay, startWith, switchMap, tap } from 'rxjs';
import { HashSuffixPipe } from '../../pipes/hash-suffix.pipe';
import { SystemService } from '../../services/system.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { Chart } from 'chart.js';  // Import Chart.js
import { ElementRef, ViewChild } from "@angular/core";
import { TimeScale } from "chart.js/auto";
import { NbThemeService } from '@nebular/theme';
import { NbTrigger } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { LocalStorageService } from '../../services/local-storage.service';
import { IPool } from 'src/app/models/IStratum';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})
export class HomeComponent implements AfterViewChecked, OnInit, OnDestroy {
  @ViewChild("myChart") ctx: ElementRef<HTMLCanvasElement>;

  protected readonly NbTrigger = NbTrigger;

  private chart: Chart;
  private themeSubscription: any;
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
  public chartData?: any;

  public hasChipTemps: boolean = false;
  public viewMode: 'gauge' | 'bars' = 'bars'; // default to bars

  private localStorageKey = 'chartData';
  private timestampKey = 'lastTimestamp'; // Key to store lastTimestamp
  private tempViewKey = 'tempViewMode';
  private legendVisibilityKey = 'chartLegendVisibility';

  public isDualPool: boolean = false;

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
    const saved = localStorage.getItem(this.legendVisibilityKey);
    if (saved) {
      const visibility = JSON.parse(saved);
      visibility.forEach((hidden: boolean, i: number) => {
        if (hidden) {
          this.chart.getDatasetMeta(i).hidden = true;
        }
      });
      this.chart.update();
    }

    this.loadChartData();
    if (this._info.history) {
      this.importHistoricalData(this._info.history);
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

    this.chartData = {
      labels: [],
      datasets: [
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1M'),
          data: this.dataData1m,
          fill: false,
          backgroundColor: '#6484f6',
          borderColor: '#6484f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_10M'),
          data: this.dataData10m,
          fill: false,
          backgroundColor: '#7464f6',
          borderColor: '#7464f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1H'),
          data: this.dataData1h,
          fill: false,
          backgroundColor: '#a564f6',
          borderColor: '#a564f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
        {
          type: 'line',
          label: this.translateService.instant('HOME.HASHRATE_1D'),
          data: this.dataData1d,
          fill: false,
          backgroundColor: '#c764f6',
          borderColor: '#c764f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
      ]
    };

    this.chartOptions = {
      animation: false,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          labels: {
            color: textColor
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
            localStorage.setItem(this.legendVisibilityKey, JSON.stringify(visibility));
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
            label: (x: any) => `${x.dataset.label}: ${HashSuffixPipe.transform(x.raw)}`
          }
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
            callback: (value: number) => HashSuffixPipe.transform(value)
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
      switchMap(() => {
        const storedLastTimestamp = this.getStoredTimestamp();
        const currentTimestamp = new Date().getTime();
        const oneHourAgo = currentTimestamp - 3600 * 1000;

        // Cap the startTimestamp to be at most one hour ago
        let startTimestamp = storedLastTimestamp ? Math.max(storedLastTimestamp + 1, oneHourAgo) : oneHourAgo;

        return this.systemService.getInfo(startTimestamp);
      }),
      tap(info => {
        if (!info) {
          return;
        }
        this._info = info;
        if (!this.chart) {
          return info;
        }
        if (info.history) {
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

  public getQuickLink(stratumURL: string, stratumUser: string): string | undefined {
    const address = stratumUser.split('.')[0];

    if (stratumURL.includes('public-pool.io')) {
      return `https://web.public-pool.io/#/app/${address}`;
    } else if (stratumURL.includes('ocean.xyz')) {
      return `https://ocean.xyz/stats/${address}`;
    } else if (stratumURL.includes('solo.d-central.tech')) {
      return `https://solo.d-central.tech/#/app/${address}`;
    } else if (/^eusolo[46]?.ckpool.org/.test(stratumURL)) {
      return `https://eusolostats.ckpool.org/users/${address}`;
    } else if (/^solo[46]?.ckpool.org/.test(stratumURL)) {
      return `https://solostats.ckpool.org/users/${address}`;
    } else if (stratumURL.includes('pool.noderunners.network')) {
      return `https://noderunners.network/en/pool/user/${address}`;
    } else if (stratumURL.includes('satoshiradio.nl')) {
      return `https://pool.satoshiradio.nl/user/${address}`;
    } else if (stratumURL.includes('solohash.co.uk')) {
      return `https://solohash.co.uk/user/${address}`;
    } else if (stratumURL.includes('parasite.wtf')) {
      return `https://parasite.space/user/${address}`;
    } else if (stratumURL.includes('solomining.de')) {
      return `https://pool.solomining.de/#/app/${address}`;
    }
    return stratumURL.startsWith('http') ? stratumURL : `http://${stratumURL}`;
  }

  public supportsPing(stratumURL: string) {
    if (stratumURL.includes('public-pool.io')) {
      return false;
    }
    return true;
  }

  ngOnInit() {
    this.themeSubscription = this.themeService.getJsTheme().subscribe(() => {
      this.updateThemeColors();
    });

    // Listen for timeFormat changes
    this.timeFormatListener = () => {
      this.updateTimeFormat();
    };
    window.addEventListener('timeFormatChanged', this.timeFormatListener);
  }

  ngOnDestroy(): void {
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
      const lastDataTimestamp = Math.max(...data.timestamps);
      this.storeTimestamp(lastDataTimestamp);
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
  }

  private updateChartData(data: any): void {
    const baseTimestamp = data.timestampBase;
    const convertedTimestamps = data.timestamps.map((ts: number) => ts + baseTimestamp);
    const convertedhashrate_1m = data.hashrate_1m.map((hr: number) => hr * 1000000000.0 / 100.0);
    const convertedhashrate_10m = data.hashrate_10m.map((hr: number) => hr * 1000000000.0 / 100.0);
    const convertedhashrate_1h = data.hashrate_1h.map((hr: number) => hr * 1000000000.0 / 100.0);
    const convertedhashrate_1d = data.hashrate_1d.map((hr: number) => hr * 1000000000.0 / 100.0);

    // Find the highest existing timestamp
    const lastTimestamp = this.dataLabel.length > 0 ? Math.max(...this.dataLabel) : -Infinity;

    // Filter new data to include only timestamps greater than the lastTimestamp
    const newData = convertedTimestamps.map((ts, index) => ({
      timestamp: ts,
      hashrate_1m: convertedhashrate_1m[index],
      hashrate_10m: convertedhashrate_10m[index],
      hashrate_1h: convertedhashrate_1h[index],
      hashrate_1d: convertedhashrate_1d[index],
    })).filter(entry => entry.timestamp > lastTimestamp);

    // Append only new data
    if (newData.length > 0) {
      this.dataLabel = [...this.dataLabel, ...newData.map(entry => entry.timestamp)];
      this.dataData1m = [...this.dataData1m, ...newData.map(entry => entry.hashrate_1m)];
      this.dataData10m = [...this.dataData10m, ...newData.map(entry => entry.hashrate_10m)];
      this.dataData1h = [...this.dataData1h, ...newData.map(entry => entry.hashrate_1h)];
      this.dataData1d = [...this.dataData1d, ...newData.map(entry => entry.hashrate_1d)];
    }
  }

  private loadChartData(): void {
    const storedData = localStorage.getItem(this.localStorageKey);
    if (storedData) {
      const parsedData = JSON.parse(storedData);
      this.dataLabel = parsedData.labels || [];
      this.dataData1m = parsedData.dataData1m || [];
      this.dataData10m = parsedData.dataData10m || [];
      this.dataData1h = parsedData.dataData1h || [];
      this.dataData1d = parsedData.dataData1d || [];
    }

    // do a simple consistency check
    this.validateOrResetHistory();

    this.updateChart();

    // make sure we load the data before we save it
    this.wasLoaded = true;
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
    };
    localStorage.setItem(this.localStorageKey, JSON.stringify(dataToSave));
  }

  private filterOldData(): void {
    const now = new Date().getTime();
    const cutoff = now - 3600 * 1000;

    while (this.dataLabel.length && this.dataLabel[0] < cutoff) {
      this.dataLabel.shift();
      this.dataData1m.shift();
      this.dataData10m.shift();
      this.dataData1h.shift();
      this.dataData1d.shift();
    }

    if (this.dataLabel.length) {
      this.storeTimestamp(this.dataLabel[this.dataLabel.length - 1]);
    }
  }

  private storeTimestamp(timestamp: number): void {
    if (this.saveLock) {
      return;
    }
    localStorage.setItem(this.timestampKey, timestamp.toString());
  }

  private getStoredTimestamp(): number | null {
    const storedTimestamp = localStorage.getItem(this.timestampKey);
    if (storedTimestamp) {
      const timestamp = parseInt(storedTimestamp, 10);
      return timestamp;
    }
    return null;
  }

  private updateChart() {
    this.chartData.labels = this.dataLabel;
    this.chartData.datasets[0].data = this.dataData1m;
    this.chartData.datasets[1].data = this.dataData10m;
    this.chartData.datasets[2].data = this.dataData1h;
    this.chartData.datasets[3].data = this.dataData1d;

    if (!this.chart) {
      return;
    }

    // Force dataset updates
    this.chart.data.datasets.forEach(dataset => dataset.data = [...dataset.data]);

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


  // edge case where chart data in the browser is not consistent
  // this happens when adding new charts
  private validateOrResetHistory() {
    const lenLabels = this.dataLabel.length;
    const len1m = this.dataData1m.length;
    const len10m = this.dataData10m.length;
    const len1h = this.dataData1h.length;
    const len1d = this.dataData1d.length;

    const lengths = [lenLabels, len1m, len10m, len1h, len1d];

    // if all arrays have the same length everything is fine
    const allEqual = lengths.every(l => l === lengths[0]);
    if (allEqual) {
      return;
    }

    // if not we clear the data and trigger a reload
    console.warn('[History] Inconsistent lengths detected from', {
      lenLabels, len1m, len10m, len1h, len1d,
    });

    // Clear in-memory history arrays
    this.dataLabel = [];
    this.dataData1m = [];
    this.dataData10m = [];
    this.dataData1h = [];
    this.dataData1d = [];

    // prevent saving anything after we clear and reload the window
    this.saveLock = true;

    // Clear persisted history
    localStorage.removeItem(this.localStorageKey);
    localStorage.removeItem(this.timestampKey);

    // Hard reload to force a clean state
    window.location.reload();
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

}

Chart.register(TimeScale);
