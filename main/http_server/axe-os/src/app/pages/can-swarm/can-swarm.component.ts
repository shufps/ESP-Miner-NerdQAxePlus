import { Component, OnInit, OnDestroy } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { combineLatest, interval, Subscription } from 'rxjs';
import { switchMap, catchError } from 'rxjs/operators';
import { of } from 'rxjs';
import { ISystemInfo } from '../../models/ISystemInfo';

export interface CanSlave {
  id: number;
  mac: string;
  active: boolean;
  foreign: boolean;
  hashRate: number;
  temp: number;
  vrTemp: number;
  asicTemps: number[];
  fanRpm: number;
  fanRpm2: number;
  fanSpeed: number;
  fanSpeed2: number;
  power: number;
  current: number;
  coreVoltageActual: number;
  shutdown: boolean;
  isMaster?: boolean;
}

@Component({
  selector: 'app-can-swarm',
  templateUrl: './can-swarm.component.html',
  styleUrls: ['./can-swarm.component.scss'],
})
export class CanSwarmComponent implements OnInit, OnDestroy {
  rows: CanSlave[] = [];
  loading = true;
  private pollSub?: Subscription;

  get slaves(): CanSlave[] {
    return this.rows.filter(r => !r.isMaster);
  }

  get totalHashRate(): number {
    return this.rows.filter(r => r.active && !r.shutdown).reduce((sum, r) => sum + r.hashRate, 0);
  }

  get totalPower(): number {
    return this.rows.filter(r => r.active).reduce((sum, r) => sum + r.power, 0);
  }

  // W/TH
  get efficiency(): number {
    const hr = this.totalHashRate;
    if (hr <= 0) return 0;
    return this.totalPower / (hr / 1000);
  }

  get activeCount(): number {
    return this.rows.filter(r => r.active).length;
  }

  constructor(private http: HttpClient) {}

  ngOnInit(): void {
    this.refresh();

    this.pollSub = interval(2000).pipe(
      switchMap(() => this.fetchAll())
    ).subscribe(rows => {
      this.rows = rows;
      this.loading = false;
    });
  }

  ngOnDestroy(): void {
    this.pollSub?.unsubscribe();
  }

  private fetchAll() {
    return combineLatest([
      this.http.get<ISystemInfo>('/api/system/info').pipe(catchError(() => of(null))),
      this.http.get<CanSlave[]>('/api/can/slaves').pipe(catchError(() => of([] as CanSlave[]))),
    ]).pipe(
      switchMap(([info, slaves]) => of(this.buildRows(info, slaves)))
    );
  }

  private refresh(): void {
    this.fetchAll().subscribe(rows => {
      this.rows = rows;
      this.loading = false;
    });
  }

  private buildRows(info: ISystemInfo | null, slaves: CanSlave[]): CanSlave[] {
    const slavesHr = slaves.filter(s => s.active && !s.shutdown).reduce((sum, s) => sum + s.hashRate, 0);
    const masterHr = Math.max(0, (info?.hashRate ?? 0) - slavesHr);
    const masterRow: CanSlave = {
      id: 0,
      mac: info?.macAddr ?? '—',
      active: true,
      foreign: false,
      isMaster: true,
      hashRate: masterHr,
      temp: info?.temp ?? 0,
      vrTemp: info?.vrTemp ?? 0,
      asicTemps: info?.asicTemps ?? [0, 0, 0, 0],
      fanRpm: info?.fanrpm ?? 0,
      fanRpm2: info?.fanrpm2 ?? 0,
      fanSpeed: info?.fanspeed ?? 0,
      fanSpeed2: info?.fanspeed2 ?? 0,
      power: info?.power ?? 0,
      current: info?.current ?? 0,
      coreVoltageActual: info?.coreVoltageActual ?? 0,
      shutdown: info?.shutdown ?? false,
    };
    return [masterRow, ...slaves];
  }

  deleteSlave(id: number): void {
    if (!confirm(`Remove slave ${id} from registry?`)) return;
    this.http.delete(`/api/can/slaves/${id}`).pipe(
      catchError(() => of(null))
    ).subscribe(() => {
      this.rows = this.rows.filter(r => r.id !== id);
    });
  }
}
