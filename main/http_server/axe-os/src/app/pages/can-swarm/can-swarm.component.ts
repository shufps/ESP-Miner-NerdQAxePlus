import { Component, OnInit, OnDestroy } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { interval, Subscription } from 'rxjs';
import { switchMap, catchError } from 'rxjs/operators';
import { of } from 'rxjs';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { SystemService } from '../../services/system.service';

export interface CanSlaveFan {
  mode: number;
  manualSpeed: number;
  overheatTemp: number;
  targetTemp: number;
  rpm: number;
  speedPerc: number;
}

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
  fans?: CanSlaveFan[];
  power: number;
  current: number;
  coreVoltageActual: number;
  shutdown: boolean;
  boardError?: number;
  deviceModel?: string;
  version?: string;
  isMaster?: boolean;
  // Settings
  frequency?: number;
  coreVoltage?: number;
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
  fleet = { hashRate: 0, power: 0 };
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
    return this.fleet.hashRate;
  }

  get totalPower(): number {
    return this.fleet.power;
  }

  get efficiency(): number {
    const hr = this.totalHashRate;
    if (hr <= 0) return 0;
    return this.totalPower / (hr / 1000);
  }

  get activeCount(): number {
    return this.rows.filter(r => r.active).length;
  }

  otpEnabled = false;

  constructor(private http: HttpClient, private fb: FormBuilder, private otpAuth: OtpAuthService, private systemService: SystemService) {}

  ngOnInit(): void {
    this.systemService.getIdentifyV2().pipe(catchError(() => of(null))).subscribe(info => {
      this.otpEnabled = !!info?.otp;
    });

    this.refresh();

    this.pollSub = interval(2000).pipe(
      switchMap(() => this.fetchNodes())
    ).subscribe(res => {
      this.fleet = res.fleet;
      this.mergeRows(res.nodes);
      this.loading = false;
    });
  }

  ngOnDestroy(): void {
    this.pollSub?.unsubscribe();
  }

  private fetchNodes() {
    return this.http.get<{ fleet: { hashRate: number; power: number }; nodes: CanSlave[] }>('/api/v2/can/nodes').pipe(
      catchError(() => of({ fleet: { hashRate: 0, power: 0 }, nodes: [] as CanSlave[] }))
    );
  }

  private refresh(): void {
    this.fetchNodes().subscribe(res => {
      this.fleet = res.fleet;
      this.mergeRows(res.nodes);
      this.loading = false;
    });
  }

  private mergeRows(incoming: CanSlave[]): void {
    incoming.forEach(newRow => {
      const existing = this.rows.find(r => r.id === newRow.id);
      if (existing) {
        Object.assign(existing, newRow);
      } else {
        this.rows.push(newRow);
      }
    });
    this.rows = this.rows.filter(r => incoming.some(nr => nr.id === r.id));
  }

  editSlave(s: CanSlave): void {
    this.editingId = s.id;
    const fan0 = s.fans?.[0];
    const fan1 = s.fans?.[1];
    this.editForm = this.fb.group({
      frequency:      [s.frequency ?? 500,  [Validators.required, Validators.min(100), Validators.max(1200)]],
      coreVoltage:    [s.coreVoltage ?? 1150, [Validators.required, Validators.min(1000), Validators.max(1400)]],
      fan0Mode:       [fan0?.mode ?? 1],
      fan0Speed:      [fan0?.manualSpeed ?? 75,    [Validators.min(0), Validators.max(100)]],
      fan0TargetTemp: [fan0?.targetTemp ?? 55,     [Validators.min(30), Validators.max(90)]],
      fan0Overheat:   [fan0?.overheatTemp ?? 75,   [Validators.min(40), Validators.max(100)]],
      fan1Mode:       [fan1?.mode ?? 3],
      fan1Speed:      [fan1?.manualSpeed ?? 100,   [Validators.min(0), Validators.max(100)]],
      fan1TargetTemp: [fan1?.targetTemp ?? 65,     [Validators.min(30), Validators.max(90)]],
      fan1Overheat:   [fan1?.overheatTemp ?? 80,   [Validators.min(40), Validators.max(100)]],
      flipScreen:     [s.flipScreen ?? false],
      autoScreenOff:  [s.autoScreenOff ?? false],
    });
  }

  cancelEdit(): void {
    this.editingId = null;
  }

  private headers(totp?: string): HttpHeaders {
    let h = new HttpHeaders();
    if (totp) h = h.set('X-TOTP', totp);
    return h;
  }

  saveSlave(id: number): void {
    if (!this.editForm.valid) return;
    this.saving = true;
    const v = this.editForm.value;
    const body = {
      frequency:    v.frequency,
      coreVoltage:  v.coreVoltage,
      fans: [
        { mode: v.fan0Mode, manualSpeed: v.fan0Speed, targetTemp: v.fan0TargetTemp, overheatTemp: v.fan0Overheat },
        { mode: v.fan1Mode, manualSpeed: v.fan1Speed, targetTemp: v.fan1TargetTemp, overheatTemp: v.fan1Overheat },
      ],
      flipScreen:   v.flipScreen,
      autoScreenOff: v.autoScreenOff,
    };
    this.otpAuth.ensureOtp$('', 'OTP Required', 'Enter your OTP to save slave settings').pipe(
      switchMap(({ totp }: EnsureOtpResult) =>
        this.http.patch(`/api/v2/can/nodes/${id}`, body, { headers: this.headers(totp) })
      ),
      catchError(() => of(null))
    ).subscribe(() => {
      this.saving = false;
      this.editingId = null;
    });
  }

  private postSlaveAction(id: number, action: 'restart' | 'shutdown' | 'identify', hint: string): void {
    this.otpAuth.ensureOtp$('', 'OTP Required', hint).pipe(
      switchMap(({ totp }: EnsureOtpResult) =>
        this.http.post(`/api/v2/can/nodes/${id}/${action}`, null, { headers: this.headers(totp) })
      ),
      catchError(() => of(null))
    ).subscribe();
  }

  restartSlave(id: number): void {
    if (!confirm(`Restart slave ${id}?`)) return;
    this.postSlaveAction(id, 'restart', 'Enter your OTP to restart slave');
  }

  shutdownSlave(id: number): void {
    if (!confirm(`Shutdown slave ${id}? It will stop mining until restarted.`)) return;
    this.postSlaveAction(id, 'shutdown', 'Enter your OTP to shut down slave');
  }

  identifySlave(id: number): void {
    this.postSlaveAction(id, 'identify', 'Enter your OTP to identify slave');
  }

  deleteSlave(id: number): void {
    if (!confirm(`Remove slave ${id} from registry?`)) return;
    this.otpAuth.ensureOtp$('', 'OTP Required', 'Enter your OTP to remove slave').pipe(
      switchMap(({ totp }: EnsureOtpResult) =>
        this.http.delete(`/api/v2/can/nodes/${id}`, { headers: this.headers(totp) })
      ),
      catchError(() => of(null))
    ).subscribe(() => {
      this.rows = this.rows.filter(r => r.id !== id);
    });
  }
}
