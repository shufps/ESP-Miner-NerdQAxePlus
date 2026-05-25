import { HttpClient, HttpEvent, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { eASICModel } from '../models/enum/eASICModel';
import { ISystemInfo } from '../models/ISystemInfo';
import { IDashboardV2 } from '../models/IDashboardV2';
import { IHistory } from '../models/IHistory';
import { IAlertSettings } from '../models/IAlertSettings';
import { AsicInfo } from '../models/IAsicInfo';
import { ISettingsV2 } from '../models/ISettingsV2';
import { IIdentifyV2 } from '../models/IIdentifyV2';
import { ISystemV2 } from '../models/ISystemV2';
import { environment } from '../../environments/environment';
import { IInfluxDB } from '../models/IInfluxDB';
import { IUpdateStatus } from '../models/IUpdateStatus';
import { HttpHeaders } from '@angular/common/http';

const defaultInfo: ISystemInfo = {
  flipscreen: 0,
  invertscreen: 0,
  autoscreenoff: 0,
  power: 0,
  minPower: 0,
  maxPower: 0,
  voltage: 0,
  maxVoltage: 0,
  minVoltage: 0,
  current: 0,
  temp: 0,
  vrTemp: 0,
  vrTempInt: 0,
  hashRateTimestamp: 0,
  hashRate: 0,
  hashRate_10m: 0,
  hashRate_1h: 0,
  hashRate_1d: 0,
  bestDiff: 0,
  bestSessionDiff: 0,
  freeHeap: 0,
  freeHeapInt: 0,
  coreVoltage: 0,
  defaultCoreVoltage: 0,
  coreVoltageActual: 0,
  hostname: "",
  hostip: "",
  macAddr: "",
  wifiRSSI: 0,
  ssid: "",
  wifiPass: "",
  wifiStatus: "",
  sharesAccepted: 0,
  sharesRejected: 0,
  uptimeSeconds: 0,
  asicCount: 0,
  smallCoreCount: 0,
  ASICModel: eASICModel.BM1368,
  deviceModel: "",
  stratumURL: "",
  stratumPort: 0,
  stratumUser: "",
  stratumEnonceSubscribe: 0,
  stratumTLS: 0,
  fallbackStratumURL: "",
  fallbackStratumPort: 0,
  fallbackStratumUser: "",
  fallbackStratumEnonceSubscribe: 0,
  fallbackStratumTLS: 0,
  stratumProtocol: 0,
  fallbackStratumProtocol: 0,
  sv2AuthorityPubkey: "",
  fallbackSv2AuthorityPubkey: "",
  sv2ChannelType: 0,
  fallbackSv2ChannelType: 0,
  frequency: 0,
  defaultFrequency: 0,
  version: "",
  invertfanpolarity: 0,
  autofanspeed: 0,
  fanspeed: 0,
  manualFanSpeed: 0,
  fanrpm: 0,
  lastResetReason: "",
  jobInterval: 0,
  stratumDifficulty: 0,
  lastpingrtt: 0,
  recentpingloss: 0,
  poolDifficulty: 0,
  stratum_keep: 0,
  vrFrequency: 0,
  defaultTheme: "",
  shutdown: false,
  stratum: {
    poolMode: 0,
    activePoolMode: 0,
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
      activeProtocol: 0,
      encrypted: false,
    }],
  },
  otp: false,
  pidTargetTemp: 0,
  pidP: 0,
  pidI: 0,
  pidD: 0,
  overheat_temp: 0,
  history: {
    hashrate_1m: [],
    hashrate_10m: [],
    hashrate_1h: [],
    hashrate_1d: [],
    vregTemp: [],
    asicTemp: [],
    hasMore: false,
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

  static defaultDashboardV2(): IDashboardV2 {
    return {
      system:      { uptime: 0, shutdown: false, boardError: 0, overheatTemp: 0 },
      performance: { hashRateTimestamp: 0, hashRate: 0, hashRate1m: 0, hashRate10m: 0, hashRate1h: 0, hashRate1d: 0, bestDiff: 0, bestSessionDiff: 0, sharesAccepted: 0, sharesRejected: 0, frequency: 0, asicCount: 0, smallCoreCount: 0 },
      power:       { watts: 0, min: 0, max: 0, voltage: 0, voltageMin: 0, voltageMax: 0, currentA: 0, currentAMin: 0, currentAMax: 0, coreVoltageActual: 0 },
      thermal:     { asicTemp: 0, vrTemp: 0, vrTempInt: 0, asicTemps: [], fans: [{ speed: 0, rpm: 0 }] },
      stratum:     { poolMode: 0, activePoolMode: 0, usingFallback: false, totalBestDiff: 0, poolBalance: 0, pools: [{ host: '', port: 0, user: '', connected: false, activeProtocol: 0, encrypted: false, accepted: 0, rejected: 0, bestDiff: 0, pingRtt: 0, pingLoss: 0, poolDifficulty: 0 }] },
      can:         { hasExtension: false, enabled: false },
      coinbase:    { blockHeaders: [], pools: [] },
      history:     { hashrate_1m: [], hashrate_10m: [], hashrate_1h: [], hashrate_1d: [], vregTemp: [], asicTemp: [], hasMore: false, timestamps: [], timestampBase: 0 },
    };
  }

  public getInfo(ts = 0, limit = 0, uri = ''): Observable<ISystemInfo> {
    let params = new HttpParams();

    if (ts > 0) {
      params = params
        .set('ts', ts)
        .set('cur', Date.now());

      if (limit > 0) {
        params = params.set('limit', limit);
      }
    }
    const endpoint = `${uri}/api/system/info`;
    return this.httpClient.get<ISystemInfo>(endpoint, { params });
  }

  // Home dashboard: request an extended history window (span) without affecting other callers.
  public getInfoWithSpan(ts = 0, limit = 0, spanMs = 0, uri = ''): Observable<ISystemInfo> {
    let params = new HttpParams();

    if (ts > 0) {
      params = params
        .set('ts', ts)
        .set('cur', Date.now());

      if (limit > 0) {
        params = params.set('limit', limit);
      }
      if (spanMs > 0) {
        params = params.set('history_span', spanMs);
      }
    }
    const endpoint = `${uri}/api/system/info`;
    return this.httpClient.get<ISystemInfo>(endpoint, { params });
  }

  // Home dashboard v2: fetches /api/v2/dashboard and returns IDashboardV2 directly.
  public getDashboardV2WithSpan(ts = 0, limit = 0, spanMs = 0): Observable<IDashboardV2> {
    let params = new HttpParams();
    if (ts > 0) {
      params = params.set('ts', ts).set('cur', Date.now());
      if (limit > 0) params = params.set('limit', limit);
      if (spanMs > 0) params = params.set('historySpan', spanMs);
    }
    return this.httpClient.get<IDashboardV2>('/api/v2/dashboard', { params });
  }

  public getAsicInfo(uri: string = ''): Observable<AsicInfo> {
    return this.httpClient.get<AsicInfo>(`${uri}/api/system/asic`);
  }

  public getSettingsV2(uri: string = ''): Observable<ISettingsV2> {
    return this.httpClient.get<ISettingsV2>(`${uri}/api/v2/settings`);
  }

  public updateSettingsV2(uri: string = '', update: any, totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);
    return this.httpClient.patch(`${uri}/api/v2/settings`, update, { headers });
  }

  public getIdentifyV2(uri: string = ''): Observable<IIdentifyV2> {
    return this.httpClient.get<IIdentifyV2>(`${uri}/api/v2/identify`);
  }

  public getSystemV2(): Observable<ISystemV2> {
    return this.httpClient.get<ISystemV2>(`/api/v2/system`);
  }

  public getInfluxInfo(uri: string = ''): Observable<IInfluxDB> {
    return this.httpClient.get(`${uri}/api/v2/influx/info`) as Observable<IInfluxDB>;
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

  public resetStats(uri: string = '') {
    return this.httpClient.post(`${uri}/api/system/reset-stats`, null, { responseType: 'text' });
  }

  public shutdown(uri: string = '', totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);

    return this.httpClient.post(`${uri}/api/system/shutdown`, null, {
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
    return this.httpClient.patch(`${uri}/api/v2/influx`, update, { headers });
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
  public performGithubOTAUpdate(url: string, keepConfig: boolean, totp?: string) {
    const headers: Record<string, string> = {};
    if (totp) headers['X-TOTP'] = totp;

    return this.httpClient.post('/api/system/OTA/github', { url, keep_config: keepConfig }, {
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
    return this.httpClient.get(`${uri}/api/v2/alert/info`) as Observable<IAlertSettings>;
  }

  // Alerts: POST /api/alert/update
  public updateAlertInfo(uri: string = '', data: IAlertSettings, totp?: string) {
    let headers = new HttpHeaders();
    if (totp) headers = headers.set('X-TOTP', totp);
    return this.httpClient.post(`${uri}/api/v2/alert/update`, data, { headers });
  }

  public sendAlertTest(uri: string = '', totp?: string): Observable<any> {
    const headers = totp ? new HttpHeaders({ 'X-OTP-Code': totp }) : undefined;
    return this.httpClient.post(`${uri}/api/v2/alert/test`, {}, {
      responseType: 'text',
      headers,
    });
  }

  /** POST /api/otp -> starts enrollment (shows QR on device) */
  public startOtpEnrollment(): Observable<void> {
    return this.httpClient.post<void>('/api/v2/otp', {}); // empty body
  }

  /** PATCH /api/otp -> {enabled:boolean, totp:string} */
  public updateOtp(enabled: boolean, totp: string): Observable<void> {
    const headers = new HttpHeaders().set('X-TOTP', totp);
    return this.httpClient.patch<void>('/api/v2/otp', { enabled }, { headers });
  }

  /** POST /api/otp/session - creates session token with expiration */
  public createOtpSession(totp: string, ttlMs?: number): Observable<{ token: string; ttlMs?: number; expiresAt?: number }> {
    let headers = new HttpHeaders({ 'X-TOTP': totp });
    if (ttlMs && ttlMs > 0) {
      headers = headers.set('X-OTP-Session-TTL', String(ttlMs));
    }
    return this.httpClient.post<{ token: string; ttlMs?: number; expiresAt?: number }>(
      '/api/v2/otp/session', {}, { headers }
    );
  }

  // only returns enabled flag
  public getOTPStatus(): Observable<{ enabled: boolean }> {
    return this.httpClient.get('/api/v2/otp/status') as Observable<{ enabled: boolean }>;
  }
}
