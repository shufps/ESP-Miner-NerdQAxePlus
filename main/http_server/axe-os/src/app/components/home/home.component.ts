import { Component } from '@angular/core';
import { interval, map, Observable, shareReplay, startWith, switchMap, tap } from 'rxjs';
import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { SystemService } from 'src/app/services/system.service';
import { ISystemInfo } from 'src/models/ISystemInfo';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})
export class HomeComponent {

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
/*
        {
          type: 'line',
          label: 'Hashrate',
          data: this.dataData,
          fill: false,
          backgroundColor: primaryColor,
          borderColor: primaryColor,
          tension: .4,
          pointRadius: 1,
          borderWidth: 1
        },
*/
        {
          type: 'line',
          label: 'Hashrate 10m',
          data: this.dataData10m,
          fill: false,
          backgroundColor: '#6484f6',
          borderColor: '#6484f6',
          tension: .4,
          pointRadius: 1,
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
          pointRadius: 1,
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
          pointRadius: 1,
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
            unit: 'hour', // Set the unit to 'minute'
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

    this.info$ = interval(5000).pipe(
      startWith(() => this.systemService.getInfo()),
      switchMap(() => this.systemService.getInfo()),
      tap(info => {

        const now = new Date().getTime();

        this.dataData.push(info.hashRate * 1000000000);
        this.dataData10m.push(info.hashRate_10m * 1000000000);
        this.dataData1h.push(info.hashRate_1h * 1000000000);
        this.dataData1d.push(info.hashRate_1d * 1000000000);
        this.dataLabel.push(now);

        if (this.dataData.length >= 1000) {
          this.dataData.shift();
          this.dataData10m.shift();
          this.dataData1h.shift();
          this.dataData1d.shift();
          this.dataLabel.shift();
        }

        this.chartData.labels = this.dataLabel;
        this.chartData.datasets[0].data = this.dataData;
        this.chartData.datasets[1].data = this.dataData1h;
        this.chartData.datasets[2].data = this.dataData1d;

        // Trigger chart update by assigning the chartData to itself
        this.chartData = {
          ...this.chartData
        };

      }),
      map(info => {
        // Adjust other system info values as necessary
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
}
