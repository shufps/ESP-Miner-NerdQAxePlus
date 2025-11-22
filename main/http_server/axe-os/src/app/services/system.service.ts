import { HttpClient, HttpEvent } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { delay, Observable, of } from 'rxjs';
import { eASICModel } from '../models/enum/eASICModel';
import { ISystemInfo } from '../models/ISystemInfo';
import { IHistory } from '../models/IHistory';
import { IAlertSettings } from '../models/IAlertSettings';
import { AsicInfo } from '../models/IAsicInfo';
import { environment } from '../../environments/environment';
import { IInfluxDB } from '../models/IInfluxDB';
import { IUpdateStatus } from '../models/IUpdateStatus';
import { HttpHeaders } from '@angular/common/http';

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
  wifiStatus: "SYSTEM.WIFI_CONNECTED",
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
  stratumEnonceSubscribe: 0,
  fallbackStratumURL: "",
  fallbackStratumPort: 3333,
  fallbackStratumUser: "",
  fallbackStratumEnonceSubscribe: 0,
  frequency: 485,
  defaultFrequency: 485,
  version: "2.0",
  flipscreen: 0,
  invertscreen: 0,
  invertfanpolarity: 0,
  autofanpolarity: 1,
  autofanspeed: 1,
  fanspeed: 100,
  manualFanSpeed: 100,
  fanrpm: 0,
  autoscreenoff: 0,
  lastResetReason: "Unknown",
  jobInterval: 1200,
  stratumDifficulty: 1000,
  lastpingrtt: 0.00,
  recentpingloss: 0.00,
  poolDifficulty: 0,
  stratum_keep: 0,
  vrFrequency: 25000,
  defaultTheme: "cosmic",
  shutdown: false,

  stratum: {
    poolMode: 0, // prim/fb
    activePoolMode: 0,
    //poolBalance: 100,
    usingFallback: false,
    totalBestDiff: 0,
    pools: [{
      connected: false,
      poolDiffErr: false,
      poolDifficulty: 0,
      accepted: 0,
      rejected: 0,
      bestDiff: 0,
      pingRtt: 0,
      pingLoss: 0,
    }],
  },

  otp: false,

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

const defaultAsicInfo: AsicInfo = {
  ASICModel: 'BM1368',
  deviceModel: 'Supra',
  asicCount: 1,
  swarmColor: 'blue',
  defaultFrequency: 490,
  defaultVoltage: 1166,
  absMaxFrequency: 800,
  absMaxVoltage: 1400,
  frequencyOptions: [400, 425, 450, 475, 485, 490, 500, 525, 550, 575],
  voltageOptions: [1100, 1150, 1166, 1200, 1250, 1300],
};

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
    return this.httpClient.get(`${uri}/api/system/info?ts=${ts}&cur=${Math.floor(Date.now())}`) as Observable<ISystemInfo>;
  }

  public getAsicInfo(uri: string = ''): Observable<AsicInfo> {
    return this.httpClient.get<AsicInfo>(`${uri}/api/system/asic`);
  }

  public getInfluxInfo(uri: string = ''): Observable<IInfluxDB> {
    return this.httpClient.get(`${uri}/api/influx/info`) as Observable<IInfluxDB>;
  }

  public getHistoryLen(): Observable<any> {
    return this.httpClient.get<any>('/api/history/len');
  }

  public getHistoryData(ts: number): Observable<any> {
    return this.httpClient.get<any>(`/api/history/data?ts=${ts}`);
  }


  // SystemService
  public restart(uri: string = '', totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);

    return this.httpClient.post(`${uri}/api/system/restart`, null, {
      headers,
      responseType: 'text', // plain text body
    });
  }


  public updateSystem(uri: string = '', update: any, totp?: string) {
    let headers = new HttpHeaders();
    if (totp) {
      headers = headers.set('X-TOTP', totp);
    }

    // Ensure uri has no trailing slash duplication (optional)
    const base = uri || '';
    const url = `${base}/api/system`;

    return this.httpClient.patch(url, update, { headers });
  }

  // Influx: PATCH /api/influx
  public updateInflux(uri: string = '', update: any, totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);
    return this.httpClient.patch(`${uri}/api/influx`, update, { headers });
  }


  // Gemeinsamer Helper für OTA-Uploads (raw bytes), optionaler TOTP-Header
  private otaUpdate(file: File | Blob, url: string, totp?: string) {
    return new Observable<HttpEvent<string>>((subscriber) => {
      const reader = new FileReader();
      //console.log(file);

      reader.onload = (event: any) => {
        const fileContent = event.target.result as ArrayBuffer;

        const headers: Record<string, string> = {
          'Content-Type': 'application/octet-stream',
        };
        if (totp) headers['X-TOTP'] = totp;

        this.httpClient.post(url, fileContent, {
          reportProgress: true,
          observe: 'events',
          responseType: 'text',
          headers,
        }).subscribe({
          next: evt => subscriber.next(evt),
          error: err => subscriber.error(err),
          complete: () => subscriber.complete(),
        });
      };
      reader.readAsArrayBuffer(file);
    });
  }

  // Legacy Firmware OTA
  public performOTAUpdate(file: File | Blob, totp?: string) {
    return this.otaUpdate(file, `/api/system/OTA`, totp);
  }

  // Legacy WWW OTA
  public performWWWOTAUpdate(file: File | Blob, totp?: string) {
    return this.otaUpdate(file, `/api/system/OTAWWW`, totp);
  }

  // GitHub One-Click OTA
  public performGithubOTAUpdate(url: string, totp?: string) {
    const headers: Record<string, string> = {};
    if (totp) headers['X-TOTP'] = totp;

    return this.httpClient.post('/api/system/OTA/github', { url }, {
      responseType: 'text',
      headers,
    });
  }

  public getGithubOTAStatus() {
    return this.httpClient.get('/api/system/OTA/github') as Observable<IUpdateStatus>;
  }


  public getSwarmInfo(uri: string = ''): Observable<{ ip: string }[]> {
    return this.httpClient.get(`${uri}/api/swarm/info`) as Observable<{ ip: string }[]>;
  }

  public updateSwarm(uri: string = '', swarmConfig: any) {
    return this.httpClient.patch(`${uri}/api/swarm`, swarmConfig);
  }


  public getAlertInfo(uri: string = ''): Observable<IAlertSettings> {
    return this.httpClient.get(`${uri}/api/alert/info`) as Observable<IAlertSettings>;
  }

  // Alerts: POST /api/alert/update
  public updateAlertInfo(uri: string = '', data: IAlertSettings, totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);
    return this.httpClient.post(`${uri}/api/alert/update`, data, { headers });
  }

  public sendAlertTest(uri: string = '', totp?: string): Observable<any> {
    const headers = totp ? new HttpHeaders({ 'X-OTP-Code': totp }) : undefined;
    return this.httpClient.post(`${uri}/api/alert/test`, {}, {
      responseType: 'text',
      headers,
    });
  }

  /** POST /api/otp -> starts enrollment (shows QR on device) */
  public startOtpEnrollment(): Observable<void> {
    return this.httpClient.post<void>('/api/otp', {}); // empty body
  }

  /** PATCH /api/otp -> {enabled:boolean, totp:string} */
  public updateOtp(enabled: boolean, totp: string): Observable<void> {
    const headers = new HttpHeaders().set('X-TOTP', totp);
    return this.httpClient.patch<void>('/api/otp', { enabled }, { headers });
  }

  /** POST /api/otp/session - creates session token with expiration */
  public createOtpSession(totp: string, ttlMs?: number): Observable<{ token: string; ttlMs?: number; expiresAt?: number }> {
    let headers = new HttpHeaders({ 'X-TOTP': totp });
    if (ttlMs && ttlMs > 0) {
      headers = headers.set('X-OTP-Session-TTL', String(ttlMs));
    }
    return this.httpClient.post<{ token: string; ttlMs?: number; expiresAt?: number }>(
      '/api/otp/session', {}, { headers }
    );
  }

  // only returns enabled flag
  public getOTPStatus(): Observable<{ enabled: boolean }> {
    return this.httpClient.get('/api/otp/status') as Observable<{ enabled: boolean }>;
  }
}

