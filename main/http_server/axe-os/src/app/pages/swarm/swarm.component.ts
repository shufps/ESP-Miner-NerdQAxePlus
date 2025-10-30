import { HttpClient } from '@angular/common/http';
import { Component, OnDestroy, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators, FormControl } from '@angular/forms';
import { catchError, forkJoin, from, map, mergeMap, of, take, timeout, toArray } from 'rxjs';
import { LocalStorageService } from '../../services/local-storage.service';
import { SystemService } from 'src/app/services/system.service';
import { NbToastrService } from '@nebular/theme';
import { LoadingService } from '../../services/loading.service';

const SWARM_DATA = 'SWARM_DATA';
const SWARM_REFRESH_TIME = 'SWARM_REFRESH_TIME';

@Component({
  selector: 'app-swarm',
  templateUrl: './swarm.component.html',
  styleUrls: ['./swarm.component.scss']
})
export class SwarmComponent implements OnInit, OnDestroy {

  public swarm: any[] = [];

  public selectedAxeOs: any = null;
  public showEdit = false;

  public form: FormGroup;

  public scanning = false;

  public refreshIntervalRef!: number;
  public refreshIntervalTime = 30;
  public refreshTimeSet = 30;

  public totals: { hashRate: number, power: number, bestDiff: string } = { hashRate: 0, power: 0, bestDiff: '0' };

  public isRefreshing = false;

  public refreshIntervalControl: FormControl;

  public ipAddress: string;

  // Legende
  public colorLegend: { color: string; label: string; count: number }[] = [];

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private localStorageService: LocalStorageService,
    private httpClient: HttpClient
  ) {
    this.form = this.fb.group({
      manualAddIp: [null, [
        Validators.required,
        Validators.pattern('(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)')
      ]]
    });

    const storedRefreshTime = this.localStorageService.getNumber(SWARM_REFRESH_TIME) ?? 30;
    this.refreshIntervalTime = storedRefreshTime;
    this.refreshTimeSet = storedRefreshTime;
    this.refreshIntervalControl = new FormControl(storedRefreshTime);

    this.refreshIntervalControl.valueChanges.subscribe(value => {
      this.refreshIntervalTime = value;
      this.refreshTimeSet = value;
      this.localStorageService.setNumber(SWARM_REFRESH_TIME, value);
    });
  }

  ngOnInit(): void {
    this.systemService.getInfo(0)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: (info) => {
          this.ipAddress = info.hostip;
          const swarmData = this.localStorageService.getObject(SWARM_DATA);

          if (swarmData == null) {
            this.scanNetwork();
          } else {
            this.swarm = swarmData;
            this.refreshList();
          }

          this.startRefreshInterval();
        },
        error: () => {
          this.startRefreshInterval(); // Start periodic refresh even if info request fails
        }
      });
  }

  ngOnDestroy(): void {
    window.clearInterval(this.refreshIntervalRef);
    this.form.reset();
  }

  private startRefreshInterval(): void {
    this.refreshIntervalRef = window.setInterval(() => {
      if (!this.scanning && !this.isRefreshing) {
        this.refreshIntervalTime--;
        if (this.refreshIntervalTime <= 0) {
          this.refreshList();
        }
      }
    }, 1000);
  }

  private ipToInt(ip: string): number {
    return ip.split('.').reduce((acc, octet) => (acc << 8) + parseInt(octet, 10), 0) >>> 0;
  }

  private intToIp(int: number): string {
    return `${(int >>> 24) & 255}.${(int >>> 16) & 255}.${(int >>> 8) & 255}.${int & 255}`;
  }

  private calculateIpRange(ip: string, netmask: string): { start: number, end: number } {
    const ipInt = this.ipToInt(ip);
    const netmaskInt = this.ipToInt(netmask);
    const network = ipInt & netmaskInt;
    const broadcast = network | ~netmaskInt;
    return { start: network + 1, end: broadcast - 1 };
  }

  // check if  /asic returns the expected fields
  private isValidAsicPayload(asic: any): boolean {
    return !!asic
      && typeof asic === 'object'
      && Array.isArray(asic.frequencyOptions)
      && Array.isArray(asic.voltageOptions)
      && ('deviceModel' in asic || 'ASICModel' in asic);
  }

  scanNetwork() {
    this.scanning = true;

    const { start, end } = this.calculateIpRange(this.ipAddress, '255.255.255.0');
    const ips = Array.from({ length: end - start + 1 }, (_, i) => this.intToIp(start + i));

    from(ips).pipe(
      mergeMap(ipAddr =>
        // get /info and /asic in parallel
        forkJoin({
          info: this.httpClient.get<any>(`http://${ipAddr}/api/system/info`).pipe(timeout(5000)),
          asic: this.httpClient.get<any>(`http://${ipAddr}/api/system/asic`).pipe(
            timeout(5000),
            catchError(() => of(null)) // /asic can be missing (302 etc.)
          )
        }).pipe(
          map(({ info, asic }) => {
            if (info && 'hashRate' in info) {
              const supportsAsicApi = this.isValidAsicPayload(asic);
              const merged = {
                IP: ipAddr,
                ...info,
                ...(supportsAsicApi ? {
                  ASICModel: asic.ASICModel ?? info.ASICModel,
                  asicCount: asic.asicCount ?? info.asicCount,
                  deviceModel: asic.deviceModel ?? info.deviceModel,
                  swarmColor: asic.swarmColor ?? 'blue',
                } : {}),
                supportsAsicApi,
              };

              merged["bestDiff"] = this.normalizeDiff(merged["bestDiff"]);
              merged["bestSessionDiff"] = this.normalizeDiff(merged["bestSessionDiff"]);

              if (!merged['swarmColor']) merged['swarmColor'] = 'blue';
              return merged;
            }
            return null;
          }),
          catchError(() => of(null))
        ),
        128
      ),
      toArray()
    ).pipe(take(1)).subscribe({
      next: (result) => {
        const validResults = result.filter((item): item is NonNullable<typeof item> => item !== null);
        const existingIps = new Set(this.swarm.map(item => item.IP));
        const newItems = validResults.filter(item => !existingIps.has(item.IP));
        this.swarm = [...this.swarm, ...newItems].sort(this.sortByIp.bind(this));
        this.localStorageService.setObject(SWARM_DATA, this.swarm);
        this.calculateTotals();
        this.rebuildColorLegend();
      },
      complete: () => {
        this.scanning = false;
      }
    });
  }

  public add() {
    const newIp = this.form.value.manualAddIp;

    if (this.swarm.some(item => item.IP === newIp)) {
      this.toastrService.warning('This IP address already exists in the swarm', 'Duplicate Entry');
      return;
    }

    forkJoin({
      info: this.systemService.getInfo(0, `http://${newIp}`),
      asic: this.httpClient.get<any>(`http://${newIp}/api/system/asic`).pipe(
        timeout(5000),
        catchError(() => of(null))
      )
    }).subscribe(({ info, asic }) => {
      if (info?.ASICModel) {
        const supportsAsicApi = this.isValidAsicPayload(asic);
        const merged = {
          IP: newIp,
          ...info,
          ...(supportsAsicApi ? {
            ASICModel: asic.ASICModel ?? info.ASICModel,
            asicCount: asic.asicCount ?? info.asicCount,
            deviceModel: asic.deviceModel ?? info.deviceModel,
            swarmColor: asic.swarmColor ?? 'blue'
          } : {}),
          supportsAsicApi,
        };
        if (!merged['swarmColor']) merged['swarmColor'] = 'blue';

        merged["bestDiff"] = this.normalizeDiff(merged["bestDiff"]);
        merged["bestSessionDiff"] = this.normalizeDiff(merged["bestSessionDiff"]);

        this.swarm.push(merged);
        this.swarm = this.swarm.sort(this.sortByIp.bind(this));
        this.localStorageService.setObject(SWARM_DATA, this.swarm);
        this.calculateTotals();
        this.rebuildColorLegend();
      }
    });
  }

  public edit(axe: any) {
    if (!axe?.supportsAsicApi) {
      this.toastrService.warning(
        'To edit settings from the Swarm page, please update this device’s firmware.',
        'Firmware Update Needed'
      );
      return;
    }
    this.selectedAxeOs = axe;
    this.showEdit = true;
  }

  public restart(axe: any) {
    this.systemService.restart(`http://${axe.IP}`).pipe(
      catchError(error => {
        this.toastrService.danger(`Failed to restart device at ${axe.IP}`, 'Error');
        return of(null);
      })
    ).subscribe(res => {
      if (res !== null) {
        this.toastrService.success(`Nerd*Axe at ${axe.IP} restarted`, 'Success');
      }
    });
  }

  public remove(axeOs: any) {
    this.swarm = this.swarm.filter(axe => axe.IP != axeOs.IP);
    this.localStorageService.setObject(SWARM_DATA, this.swarm);
    this.calculateTotals();
    this.rebuildColorLegend();
  }

  public refreshList() {
    if (this.scanning) {
      return;
    }

    this.refreshIntervalTime = this.refreshTimeSet;
    const ips = this.swarm.map(axeOs => axeOs.IP);
    this.isRefreshing = true;

    from(ips).pipe(
      mergeMap(ipAddr =>
        forkJoin({
          info: this.httpClient.get<any>(`http://${ipAddr}/api/system/info`).pipe(timeout(5000)),
          asic: this.httpClient.get<any>(`http://${ipAddr}/api/system/asic`).pipe(
            timeout(5000),
            catchError(() => of(null))
          )
        }).pipe(
          map(({ info, asic }) => {
            const existingDevice = this.swarm.find(axeOs => axeOs.IP === ipAddr);
            const supportsAsicApi = this.isValidAsicPayload(asic) || !!existingDevice?.supportsAsicApi;
            const merged = {
              IP: ipAddr,
              ...existingDevice,
              ...info,
              ...(this.isValidAsicPayload(asic) ? {
                ASICModel: asic.ASICModel ?? info?.ASICModel ?? existingDevice?.ASICModel,
                asicCount: asic.asicCount ?? info?.asicCount ?? existingDevice?.asicCount,
                deviceModel: asic.deviceModel ?? info?.deviceModel ?? existingDevice?.deviceModel,
                swarmColor: asic.swarmColor ?? info?.swarmColor ?? existingDevice?.swarmColor
              } : {}),
              supportsAsicApi,
            };
            if (!merged['swarmColor']) merged['swarmColor'] = existingDevice?.swarmColor ?? 'blue';
            return merged;
          }),
          catchError(error => {
            const errorMessage = error?.message || error?.statusText || error?.toString() || 'Unknown error';
            this.toastrService.danger('Failed to get info from ' + ipAddr, errorMessage);
            const existingDevice = this.swarm.find(axeOs => axeOs.IP === ipAddr);
            return of({
              ...existingDevice,
              IP: ipAddr,
              hashRate: 0,
              sharesAccepted: 0,
              power: 0,
              voltage: 0,
              temp: 0,
              bestDiff: 0,
              version: 0,
              uptimeSeconds: 0,
              poolDifficulty: 0,
              swarmColor: existingDevice?.swarmColor ?? 'blue',
              supportsAsicApi: existingDevice?.supportsAsicApi ?? false,
            });
          })
        ),
        128
      ),
      toArray()
    ).pipe(take(1)).subscribe({
      next: (result) => {
        this.swarm = result.sort(this.sortByIp.bind(this));
        this.localStorageService.setObject(SWARM_DATA, this.swarm);
        this.calculateTotals();
        this.rebuildColorLegend();
        this.isRefreshing = false;
      },
      complete: () => {
        this.isRefreshing = false;
      }
    });
  }

  private sortByIp(a: any, b: any): number {
    return this.ipToInt(a.IP) - this.ipToInt(b.IP);
  }

  private normalizeDiff(value: string): string {
    // Trim whitespace
    value = value.trim();

    // If it's not purely numeric, return as-is
    if (!/^[\d.]+$/.test(value)) {
      return value;
    }

    // Convert to number
    const num = parseFloat(value);
    if (!isFinite(num) || isNaN(num)) return '0.00';

    // Define SI units
    const units = ['', 'K', 'M', 'G', 'T', 'P', 'E'];
    let unitIndex = 0;
    let scaled = num;

    // Iteratively scale down
    while (scaled >= 1000 && unitIndex < units.length - 1) {
      scaled /= 1000;
      unitIndex++;
    }

    return `${scaled.toFixed(2)}${units[unitIndex]}`;
  }

  private convertBestDiffToNumber(bestDiff: string): number {
    if (!bestDiff || typeof bestDiff !== 'string') return 0;
    const value = parseFloat(bestDiff);
    if (isNaN(value)) return 0;
    const unit = bestDiff.slice(-1).toUpperCase();
    switch (unit) {
      case 'T': return value * 1_000_000_000_000;
      case 'G': return value * 1_000_000_000;
      case 'M': return value * 1_000_000;
      case 'K': return value * 1_000;
      default: return value;
    }
  }

  private formatBestDiff(value: number): string {
    if (!isFinite(value) || isNaN(value)) return '0.00';
    if (value >= 1_000_000_000_000) return `${(value / 1_000_000_000_000).toFixed(2)}T`;
    if (value >= 1_000_000_000) return `${(value / 1_000_000_000).toFixed(2)}G`;
    if (value >= 1_000_000) return `${(value / 1_000_000).toFixed(2)}M`;
    if (value >= 1_000) return `${(value / 1_000).toFixed(2)}K`;
    return value.toFixed(2);
  }

  private calculateTotals() {
    this.totals.hashRate = this.swarm.reduce((sum, axe) => sum + (axe.hashRate || 0), 0);
    this.totals.power = this.swarm.reduce((sum, axe) => sum + (axe.power || 0), 0);

    const numericDiffs = this.swarm
      .map(axe => this.convertBestDiffToNumber(axe.bestDiff))
      .filter(v => !isNaN(v) && isFinite(v));

    const maxDiff = numericDiffs.length > 0 ? Math.max(...numericDiffs) : 0;
    this.totals.bestDiff = this.formatBestDiff(maxDiff);
  }

  hasModel(model: string): string {
    return this.swarm.some(axe => axe.ASICModel === model) ? '1' : '0.5';
  }

  hasMultipleChips(): string {
    return this.swarm.some(axe => axe.asicCount > 1) ? '1' : '0.5';
  }

  // Swarm color for the template
  public getSwarmColor(axe: any): string {
    return axe?.swarmColor || 'blue';
  }

  // build legend
  private rebuildColorLegend(): void {
    const mapLegend = new Map<string, { count: number; names: Set<string> }>();
    for (const axe of this.swarm) {
      const color = (axe?.swarmColor || '#007DB4').toString().trim();
      const name  = (axe?.deviceModel || axe?.ASICModel || 'Device').toString().trim();
      const entry = mapLegend.get(color) ?? { count: 0, names: new Set<string>() };
      entry.count++;
      if (name) entry.names.add(name);
      mapLegend.set(color, entry);
    }

    this.colorLegend = [...mapLegend.entries()].map(([color, { count, names }]) => {
      const labels = [...names];
      let label: string;
      if (labels.length === 1) label = labels[0];
      else if (labels.length === 2) label = `${labels[0]} + ${labels[1]}`;
      else label = `${labels[0]} + ${labels[1]} + ${labels.length - 2} more`;
      return { color, label, count };
    })
    .sort((a, b) => a.label.localeCompare(b.label));
  }
}
