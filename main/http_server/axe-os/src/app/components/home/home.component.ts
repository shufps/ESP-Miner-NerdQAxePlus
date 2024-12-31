import { Component, OnInit, OnDestroy } from '@angular/core';
import { interval, map, Observable, shareReplay, merge, startWith, Subject, switchMap, tap, from, of } from 'rxjs';
import { concatMap } from 'rxjs/operators';
import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { SystemService } from 'src/app/services/system.service';
import { ISystemInfo } from 'src/models/ISystemInfo';
import { Chart } from 'chart.js';  // Import Chart.js

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})
export class HomeComponent implements OnInit, OnDestroy {

  public info$: Observable<ISystemInfo>;
  public quickLink$: Observable<string | undefined>;
  public expectedHashRate$: Observable<number | undefined>;

  public chartOptions: any;
  public dataLabel: number[] = [];
  public dataData: number[] = [];
  public dataData10m: number[] = [];
  public dataData1h: number[] = [];
  public dataData1d: number[] = [];
  public chartData?: any;

  private localStorageKey = 'chartData';
  private timestampKey = 'lastTimestamp'; // Key to store lastTimestamp

  constructor(
    private systemService: SystemService
  ) {
    const documentStyle = getComputedStyle(document.documentElement);
    const textColor = documentStyle.getPropertyValue('--text-color');
    const textColorSecondary = documentStyle.getPropertyValue('--text-color-secondary');
    const surfaceBorder = documentStyle.getPropertyValue('--surface-border');
    const primaryColor = documentStyle.getPropertyValue('--primary-color');

    this.chartData = {
      labels: [],
      datasets: [
        {
          type: 'line',
          label: 'Hashrate 10m',
          data: this.dataData10m,
          fill: false,
          backgroundColor: '#6484f6',
          borderColor: '#6484f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
        {
          type: 'line',
          label: 'Hashrate 1h',
          data: this.dataData1h,
          fill: false,
          backgroundColor: '#7464f6',
          borderColor: '#7464f6',
          tension: .4,
          pointRadius: 0,
          borderWidth: 1
        },
        {
          type: 'line',
          label: 'Hashrate 1d',
          data: this.dataData1d,
          fill: false,
          backgroundColor: '#a564f6',
          borderColor: '#a564f6',
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
          }
        }
      },
      scales: {
        x: {
          type: 'time',
          time: {
            unit: 'hour',
          },
          ticks: {
            color: textColorSecondary
          },
          grid: {
            color: surfaceBorder,
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
            color: surfaceBorder,
            drawBorder: false
          }
        }
      }
    };

    this.loadChartData(); // Load chart data and lastTimestamp from local storage

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
        info.power = Math.max(info.minPower, Math.min(info.maxPower, info.power));
        info.voltage = Math.max(info.minVoltage, Math.min(info.maxVoltage, info.voltage));

        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));
        info.temp = parseFloat(info.temp.toFixed(1));
        info.vrTemp = parseFloat(info.vrTemp.toFixed(1));

        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.expectedHashRate$ = this.info$.pipe(map(info => {
      if (!info) return 0; // Return 0 if no info
      return Math.floor(info.frequency * ((info.smallCoreCount * info.asicCount) / 1000));
    }));

    this.quickLink$ = this.info$.pipe(
      map(info => {
        if (!info) return undefined; // Return undefined if no info
        if (info.stratumURL.includes('public-pool.io')) {
          const address = info.stratumUser.split('.')[0];
          return `https://web.public-pool.io/#/app/${address}`;
        } else if (info.stratumURL.includes('ocean.xyz')) {
          const address = info.stratumUser.split('.')[0];
          return `https://ocean.xyz/stats/${address}`;
        } else if (info.stratumURL.includes('solo.d-central.tech')) {
          const address = info.stratumUser.split('.')[0];
          return `https://solo.d-central.tech/#/app/${address}`;
        } else if (/solo[46]?\.ckpool\.org/.test(info.stratumURL)) {
          const address = info.stratumUser.split('.')[0];
          return `https://solo.ckpool.org/users/${address}`;
        } else {
          return undefined;
        }
      })
    );
  }

  ngOnInit(): void {
  }

  ngOnDestroy(): void {
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
    this.saveChartData();

    // set flag that we have finished the initial import
    this.updateChart();
  }

  private clearChartData(): void {
    this.dataLabel = [];
    this.dataData10m = [];
    this.dataData1h = [];
    this.dataData1d = [];
  }

  private updateChartData(data: any): void {
    const baseTimestamp = data.timestampBase;
    const convertedTimestamps = data.timestamps.map((ts: number) => ts + baseTimestamp);
    const convertedhashrate_10m = data.hashrate_10m.map((hr: number) => hr * 1000000000.0 / 100.0);
    const convertedhashrate_1h = data.hashrate_1h.map((hr: number) => hr * 1000000000.0 / 100.0);
    const convertedhashrate_1d = data.hashrate_1d.map((hr: number) => hr * 1000000000.0 / 100.0);

    this.dataLabel = [...this.dataLabel, ...convertedTimestamps];
    this.dataData10m = [...this.dataData10m, ...convertedhashrate_10m];
    this.dataData1h = [...this.dataData1h, ...convertedhashrate_1h];
    this.dataData1d = [...this.dataData1d, ...convertedhashrate_1d];
  }

  private loadChartData(): void {
    const storedData = localStorage.getItem(this.localStorageKey);
    if (storedData) {
      const parsedData = JSON.parse(storedData);
      this.dataLabel = parsedData.labels || [];
      this.dataData10m = parsedData.dataData10m || [];
      this.dataData1h = parsedData.dataData1h || [];
      this.dataData1d = parsedData.dataData1d || [];
    }
  }

  private saveChartData(): void {
    const dataToSave = {
      labels: this.dataLabel,
      dataData10m: this.dataData10m,
      dataData1h: this.dataData1h,
      dataData1d: this.dataData1d
    };
    localStorage.setItem(this.localStorageKey, JSON.stringify(dataToSave));
  }

  private filterOldData(): void {
    const now = new Date().getTime();
    const cutoff = now - 3600 * 1000;

    while (this.dataLabel.length && this.dataLabel[0] < cutoff) {
      this.dataLabel.shift();
      this.dataData10m.shift();
      this.dataData1h.shift();
      this.dataData1d.shift();
    }

    if (this.dataLabel.length) {
      this.storeTimestamp(this.dataLabel[this.dataLabel.length - 1]);
    }
  }

  private storeTimestamp(timestamp: number): void {
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
    this.chartData.datasets[0].data = this.dataData10m;
    this.chartData.datasets[1].data = this.dataData1h;
    this.chartData.datasets[2].data = this.dataData1d;

    this.chartData = {
      ...this.chartData
    };
  }
}
