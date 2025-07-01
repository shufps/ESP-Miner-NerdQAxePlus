import { HttpClient, HttpEvent } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { delay, Observable, of } from 'rxjs';
import { eASICModel } from '../models/enum/eASICModel';
import { ISystemInfo } from '../models/ISystemInfo';
import { IHistory } from '../models/IHistory';
import { IAlertSettings } from '../models/IAlertSettings';


import { environment } from '../../environments/environment';
import { IInfluxDB } from '../models/IInfluxDB';

const defaultInfo: ISystemInfo = {
  power: 11.670000076293945,
  minPower: 5.0,
  maxPower: 15.0,
  voltage: 5208.75,
  maxVoltage: 4.5,
  minVoltage: 5.5,
  current: 2237.5,
  temp: 60,
  vrTemp: 45,
  hashRateTimestamp: 1724398272483,
  hashRate: 475,
  hashRate_10m: 475,
  hashRate_1h: 475,
  hashRate_1d: 475,
  bestDiff: "0",
  bestSessionDiff: "0",
  freeHeap: 8388608,
  freeHeapInt: 102400,
  coreVoltage: 1200,
  defaultCoreVoltage: 1200,
  coreVoltageActual: 1200,
  hostname: "Bitaxe",
  hostip: "192.168.0.123",
  macAddr: "DE:AD:C0:DE:0B:7C",
  wifiRSSI: -90,
  ssid: "default",
  wifiPass: "password",
  wifiStatus: "Connected!",
  sharesAccepted: 1,
  sharesRejected: 0,
  uptimeSeconds: 38,
  asicCount: 1,
  smallCoreCount: 672,
  ASICModel: eASICModel.BM1368,
  deviceModel: "NerdQAxe+",
  stratumURL: "public-pool.io",
  stratumPort: 21496,
  stratumUser: "bc1q99n3pu025yyu0jlywpmwzalyhm36tg5u37w20d.bitaxe-U1",
  fallbackStratumURL: "",
  fallbackStratumPort: 3333,
  fallbackStratumUser: "",
  isUsingFallbackStratum: false,
  isStratumConnected: false,
  frequency: 485,
  defaultFrequency: 485,
  version: "2.0",
  flipscreen: 0,
  invertscreen: 0,
  invertfanpolarity: 0,
  autofanpolarity: 1,
  autofanspeed: 1,
  fanspeed: 100,
  fanrpm: 0,
  autoscreenoff: 0,
  lastResetReason: "Unknown",
  jobInterval: 1200,
  stratumDifficulty: 1000,
  lastpingrtt: 0.00,
  poolDifficulty: 0,

  pidTargetTemp: 55,
  pidP: 2.0,
  pidI: 0.1,
  pidD: 5.0,

  boardtemp1: 30,
  boardtemp2: 40,
  overheat_temp: 70,
  history: {
    hashrate_10m: [],
    hashrate_1h: [],
    hashrate_1d: [],
    timestamps: [],
    timestampBase: 0
  }
}


@Injectable({
  providedIn: 'root'
})
export class SystemService {

  constructor(
    private httpClient: HttpClient
  ) { }

  static defaultInfo() {
    return defaultInfo;
  }

  public getInfo(ts: number, uri: string = ''): Observable<ISystemInfo> {
    if (environment.production) {
      return this.httpClient.get(`${uri}/api/system/info?ts=${ts}&cur=${Math.floor(Date.now())}`) as Observable<ISystemInfo>;
    } else {
      return of(defaultInfo).pipe(delay(1000));
    }
  }

  public getInfluxInfo(uri: string = ''): Observable<IInfluxDB> {
    if (environment.production) {
      return this.httpClient.get(`${uri}/api/influx/info`) as Observable<IInfluxDB>;
    } else {
      return of(
        {
          influxEnable: 0,
          influxURL: "http://192.168.0.1",
          influxPort: 8086,
          influxToken: "TOKEN",
          influxBucket: "BUCKET",
          influxOrg: "ORG",
          influxPrefix: "mainnet_stats"
        }
      ).pipe(delay(1000));
    }
  }

  public getHistoryLen(): Observable<any> {
    return this.httpClient.get<any>('/api/history/len');
  }

  public getHistoryData(ts: number): Observable<any> {
    return this.httpClient.get<any>(`/api/history/data?ts=${ts}`);
  }


  public restart(uri: string = '') {
    return this.httpClient.post(`${uri}/api/system/restart`, {}, { responseType: 'text' });
  }

  public updateSystem(uri: string = '', update: any) {
    return this.httpClient.patch(`${uri}/api/system`, update);
  }

  public updateInflux(uri: string = '', update: any) {
    return this.httpClient.patch(`${uri}/api/influx`, update);
  }


  private otaUpdate(file: File | Blob, url: string) {
    return new Observable<HttpEvent<string>>((subscriber) => {
      const reader = new FileReader();

      reader.onload = (event: any) => {
        const fileContent = event.target.result;

        return this.httpClient.post(url, fileContent, {
          reportProgress: true,
          observe: 'events',
          responseType: 'text', // Specify the response type
          headers: {
            'Content-Type': 'application/octet-stream', // Set the content type
          },
        }).subscribe({
          next: (event) => {
            subscriber.next(event);
          },
          error: (err) => {
            subscriber.error(err)
          },
          complete: () => {
            subscriber.complete();
          }
        });
      };
      reader.readAsArrayBuffer(file);
    });
  }


  public performOTAUpdate(file: File | Blob) {
    return this.otaUpdate(file, `/api/system/OTA`);
  }
  public performWWWOTAUpdate(file: File | Blob) {
    return this.otaUpdate(file, `/api/system/OTAWWW`);
  }


  public getSwarmInfo(uri: string = ''): Observable<{ ip: string }[]> {
    return this.httpClient.get(`${uri}/api/swarm/info`) as Observable<{ ip: string }[]>;
  }

  public updateSwarm(uri: string = '', swarmConfig: any) {
    return this.httpClient.patch(`${uri}/api/swarm`, swarmConfig);
  }


  public getAlertInfo(uri: string = ''): Observable<IAlertSettings> {
    if (environment.production) {
      return this.httpClient.get(`${uri}/api/alert/info`) as Observable<IAlertSettings>;
    } else {
      return of({
        alertDiscordEnable: 0,
        alertDiscordWebhook: 'https://discord.com/api/webhooks/xxx/yyy'
      }).pipe(delay(1000));
    }
  }

  public updateAlertInfo(uri: string = '', data: IAlertSettings): Observable<any> {
    if (environment.production) {
      return this.httpClient.post(`${uri}/api/alert/update`, data);
    } else {
      console.log('Mock updateAlertInfo called:', data);
      return of(true).pipe(delay(500));
    }
  }

  public sendAlertTest(uri: string = ''): Observable<any> {
    if (environment.production) {
      return this.httpClient.post(`${uri}/api/alert/test`, {}, {responseType: 'text' });
    } else {
      console.log('Mock sendAlertTest');
      return of(true).pipe(delay(500));
    }
  }
}

