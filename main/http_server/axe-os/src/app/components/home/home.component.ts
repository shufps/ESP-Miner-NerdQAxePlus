import { Component, OnInit, OnDestroy } from '@angular/core';
import { interval, map, Observable, shareReplay, startWith, switchMap, tap, from } from 'rxjs';
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
  private isHistoricalDataLoaded: boolean = false; // Flag to indicate if historical data has been loaded
  private last_timestamp : Number = 0;

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
      switchMap(() => this.isHistoricalDataLoaded ? this.systemService.getInfo() : []),
      tap(info => {
        if (!info) return; // Skip if no info is available (before historical data is loaded)

        const timestamp = info.hashRateTimestamp;

        // do we have new data?
        if (timestamp == this.last_timestamp) {
          return;
        }
        this.last_timestamp = timestamp;

        // keep only 1h
        this.filterOldData();

        this.dataData10m.push(info.hashRate_10m * 1000000000);
        this.dataData1h.push(info.hashRate_1h * 1000000000);
        this.dataData1d.push(info.hashRate_1d * 1000000000);
        this.dataLabel.push(timestamp);

        this.updateChart();
        this.saveChartData();
        this.storeTimestamp(timestamp);
      }),
      map(info => {
        info.power = parseFloat(info.power.toFixed(1));
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));
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
      return Math.floor(info.frequency * ((info.smallCoreCount * info.asicCount) / 1000));
    }));

    this.quickLink$ = this.info$.pipe(
      map(info => {
        if (info.stratumURL.includes('public-pool.io')) {
          const address = info.stratumUser.split('.')[0];
          return `https://web.public-pool.io/#/app/${address}`;
        } else if (info.stratumURL.includes('ocean.xyz')) {
          const address = info.stratumUser.split('.')[0];
          return `https://ocean.xyz/stats/${address}`;
        } else if (info.stratumURL.includes('solo.d-central.tech')) {
          const address = info.stratumUser.split('.')[0];
          return `https://solo.d-central.tech/#/app/${address}`;
        } else if (info.stratumURL.includes('solo.ckpool.org')) {
          const address = info.stratumUser.split('.')[0];
          return `https://solostats.ckpool.org/stats/${address}`;
        } else {
          return undefined;
        }
      })
    );
  }

  ngOnInit(): void {
    const storedLastTimestamp = this.getStoredTimestamp();
    const currentTimestamp = new Date().getTime();
    const oneHourAgo = currentTimestamp - 3600 * 1000;

    //console.log("stored timestamp: " + storedLastTimestamp);

    let startTimestamp = storedLastTimestamp ? storedLastTimestamp + 1 : oneHourAgo;
    //console.log("start fetching timestamp: " + startTimestamp);

    const endTimestamp = currentTimestamp; // Fetch data up to current time
    //console.log("end fetching timestamp: " + endTimestamp);

    if (startTimestamp < endTimestamp) {
        this.fetchHistoricalData(startTimestamp, endTimestamp);
    } else {
        //console.log('No new data to fetch');
        this.filterOldData(); // Call filterOldData if no new data to ensure old data cleanup
        this.isHistoricalDataLoaded = true; // Set flag to true to start live data updates
    }
  }

  ngOnDestroy(): void {
    // Optionally clear any intervals or subscriptions if necessary
  }

  private fetchHistoricalData(startTimestamp: number, lastTimestamp: number): void {
    const requests: Observable<any>[] = [];
    requests.push(this.systemService.getHistoryData(startTimestamp));
    //console.log("Fetching historical data from " + startTimestamp);

    from(requests).pipe(
      concatMap((request: Observable<any>) => request),
      tap(data => {
        //console.log('Received Historical Data:', data);
        this.updateChartData(data);

        if (data.timestamps && data.timestamps.length) {
          const lastDataTimestamp = Math.max(...data.timestamps);
          this.storeTimestamp(lastDataTimestamp);
          //console.log("store new timestamp: " + lastDataTimestamp);
        }

        this.filterOldData();
        this.saveChartData();
        this.isHistoricalDataLoaded = true; // Set flag to true after historical data is processed
      })
    ).subscribe({
      complete: () => {
        //console.log('All historical data has been retrieved.');
        this.updateChart();
      }
    });
  }

  private clearChartData(): void {
    this.dataLabel = [];
    this.dataData10m = [];
    this.dataData1h = [];
    this.dataData1d = [];
  }

  private updateChartData(data: any): void {
    const convertedhashrate_10m = data.hashrate_10m.map((hr: number) => hr * 1000000000);
    const convertedhashrate_1h = data.hashrate_1h.map((hr: number) => hr * 1000000000);
    const convertedhashrate_1d = data.hashrate_1d.map((hr: number) => hr * 1000000000);

    this.dataLabel = [...this.dataLabel, ...data.timestamps];
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
    //console.log("store new timestamp: " + timestamp);
    localStorage.setItem(this.timestampKey, timestamp.toString());
  }

  private getStoredTimestamp(): number | null {
    const storedTimestamp = localStorage.getItem(this.timestampKey);
    if (storedTimestamp) {
      const timestamp = parseInt(storedTimestamp, 10);
      //console.log("loaded stored timestamp: " + timestamp);
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
