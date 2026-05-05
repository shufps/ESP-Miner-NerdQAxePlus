import { Component, OnInit, OnDestroy } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
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
  boardError?: number;
  deviceModel?: string;
  fwVersion?: string;
  freeHeap?: number;
  isMaster?: boolean;
  // Settings (from telemetry v2)
  freqMhz?: number;
  voltageMv?: number;
  fan0Mode?: number;
  fan0Speed?: number;
  fan0TargetTemp?: number;
  fan0Overheat?: number;
  fan1Mode?: number;
  fan1Speed?: number;
  fan1TargetTemp?: number;
  fan1Overheat?: number;
  flipScreen?: boolean;
  autoScreenOff?: boolean;
}

// Mirrors Board::Error enum order from board.h
export const BOARD_ERROR_STRINGS: Record<number, string> = {
  0: '',
  1: 'MINER OVERHEATED',
  2: 'VREG OVERHEATED',
  3: 'PSU FAULT',
  4: 'CURRENT PROTECTION',
  5: 'VOLTAGE PROTECTION',
};

export function boardErrorStr(code: number | undefined): string {
  if (!code) return '';
  return BOARD_ERROR_STRINGS[code] ?? 'ERROR';
}

export const FAN_MODES = [
  { value: 0, label: 'Manual' },
  { value: 2, label: 'PID' },
  { value: 3, label: 'Linked' },
];

@Component({
  selector: 'app-can-swarm',
  templateUrl: './can-swarm.component.html',
  styleUrls: ['./can-swarm.component.scss'],
})
export class CanSwarmComponent implements OnInit, OnDestroy {
  rows: CanSlave[] = [];
  loading = true;
  fanModes = FAN_MODES;

  editingId: number | null = null;
  editForm!: FormGroup;
  saving = false;
  boardErrorStr = boardErrorStr;

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

  get efficiency(): number {
    const hr = this.totalHashRate;
    if (hr <= 0) return 0;
    return this.totalPower / (hr / 1000);
  }

  get activeCount(): number {
    return this.rows.filter(r => r.active).length;
  }

  constructor(private http: HttpClient, private fb: FormBuilder) {}

  ngOnInit(): void {
    this.refresh();

    this.pollSub = interval(2000).pipe(
      switchMap(() => this.fetchAll())
    ).subscribe(rows => {
      this.mergeRows(rows);
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
      this.mergeRows(rows);
      this.loading = false;
    });
  }

  private mergeRows(incoming: CanSlave[]): void {
    // Update existing row objects in-place so Angular doesn't recreate DOM nodes
    // (preserves input focus during polling).
    incoming.forEach(newRow => {
      const existing = this.rows.find(r => r.id === newRow.id);
      if (existing) {
        Object.assign(existing, newRow);
      } else {
        this.rows.push(newRow);
      }
    });
    // Remove rows that disappeared
    this.rows = this.rows.filter(r => incoming.some(nr => nr.id === r.id));
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
      deviceModel: info?.deviceModel ?? undefined,
      fwVersion: info?.version ?? undefined,
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

  editSlave(s: CanSlave): void {
    this.editingId = s.id;
    this.editForm = this.fb.group({
      freqMhz:      [s.freqMhz ?? 500, [Validators.required, Validators.min(100), Validators.max(1200)]],
      voltageMv:    [s.voltageMv ?? 1150, [Validators.required, Validators.min(1000), Validators.max(1400)]],
      fan0Mode:     [s.fan0Mode ?? 1],
      fan0Speed:    [s.fan0Speed ?? 75, [Validators.min(0), Validators.max(100)]],
      fan0TargetTemp: [s.fan0TargetTemp ?? 55, [Validators.min(30), Validators.max(90)]],
      fan0Overheat: [s.fan0Overheat ?? 75, [Validators.min(40), Validators.max(100)]],
      fan1Mode:     [s.fan1Mode ?? 3],
      fan1Speed:    [s.fan1Speed ?? 100, [Validators.min(0), Validators.max(100)]],
      fan1TargetTemp: [s.fan1TargetTemp ?? 65, [Validators.min(30), Validators.max(90)]],
      fan1Overheat: [s.fan1Overheat ?? 80, [Validators.min(40), Validators.max(100)]],
      flipScreen:   [s.flipScreen ?? false],
      autoScreenOff:[s.autoScreenOff ?? false],
    });
  }

  cancelEdit(): void {
    this.editingId = null;
  }

  saveSlave(id: number): void {
    if (!this.editForm.valid) return;
    this.saving = true;
    const v = this.editForm.value;
    const body = {
      freqMhz:   v.freqMhz,
      voltageMv: v.voltageMv,
      fan0: { mode: v.fan0Mode, speed: v.fan0Speed, targetTemp: v.fan0TargetTemp, overheat: v.fan0Overheat },
      fan1: { mode: v.fan1Mode, speed: v.fan1Speed, targetTemp: v.fan1TargetTemp, overheat: v.fan1Overheat },
      flipScreen:   v.flipScreen,
      autoScreenOff: v.autoScreenOff,
    };
    this.http.patch(`/api/can/slaves/${id}`, body).pipe(
      catchError(() => of(null))
    ).subscribe(() => {
      this.saving = false;
    });
  }

  restartSlave(id: number): void {
    if (!confirm(`Restart slave ${id}?`)) return;
    this.http.patch(`/api/can/slaves/${id}`, { restart: true }).pipe(
      catchError(() => of(null))
    ).subscribe();
  }

  shutdownSlave(id: number): void {
    if (!confirm(`Shutdown slave ${id}? It will stop mining until restarted.`)) return;
    this.http.patch(`/api/can/slaves/${id}`, { shutdown: true }).pipe(
      catchError(() => of(null))
    ).subscribe();
  }

  identifySlave(id: number): void {
    this.http.patch(`/api/can/slaves/${id}`, { identify: true }).pipe(
      catchError(() => of(null))
    ).subscribe();
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
