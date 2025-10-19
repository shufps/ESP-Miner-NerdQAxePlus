import { Component, Inject, OnDestroy, OnInit, Input } from '@angular/core';
import { NB_DIALOG_CONFIG, NbDialogRef } from '@nebular/theme';
import { Subscription, take } from 'rxjs';
import { SystemService } from '../../services/system.service';

export interface OtpDialogResult {
  code?: string;
  remember24h: boolean;
  session?: { token: string; ttlMs?: number; expiresAt?: number };
}

export interface OtpDialogConfig {
  dialogTitle?: string;   // <— umbenannt (kein Konflikt mit getter)
  dialogHint?: string;    // <— umbenannt
  periodSec?: number;
  otpEnabled?: boolean;
  ttlMs?: number;
}

@Component({
  selector: 'app-otp-dialog',
  templateUrl: './otp-dialog.component.html',
  styleUrls: ['./otp-dialog.component.scss'],
})
export class OtpDialogComponent implements OnInit, OnDestroy {
  // Diese Felder darf Nebular via context überschreiben:
  @Input() dialogTitle?: string;
  @Input() dialogHint?: string;
  @Input() ttlMs?: number;
  @Input() otpEnabled?: boolean;

  code = '';
  remember24h = false;
  pending = false;

  private sub?: Subscription;

  constructor(
    private ref: NbDialogRef<OtpDialogComponent>,
    @Inject(NB_DIALOG_CONFIG) public cfg: OtpDialogConfig,
    private system: SystemService,
  ) {}

  ngOnInit(): void {
    // request from backend if otpEnabled not provided
    if (typeof this.otpEnabled !== 'boolean') {
      this.sub = this.system.getOTPStatus().pipe(take(1)).subscribe({
        next: status => (this.otpEnabled = !!status?.enabled),
        error: () => (this.otpEnabled = true),
      });
    }
  }

  ngOnDestroy() { this.sub?.unsubscribe(); }

  onInput() { this.code = this.code.replace(/\D+/g, '').slice(0, 6); }
  onKeydown(ev: KeyboardEvent) {
    if (ev.key === 'Enter' && this.code.length === 6) this.submit();
    if (ev.key === 'Escape') this.cancel();
  }

  cancel() { this.ref.close(null); }

  submit() {
    // when otp not enabled, just close
    if (!this.otpEnabled) {
      this.ref.close(<OtpDialogResult>{ code: this.code, remember24h: false });
      return;
    }

    // „Don’t ask for 24h“ -> request session
    if (this.remember24h) {
      if (this.code.length !== 6) return;
      this.pending = true;
      const ttl = this.ttlMs ?? this.cfg?.ttlMs ?? 24 * 60 * 60 * 1000;

      this.system.createOtpSession(this.code, ttl).pipe(take(1)).subscribe({
        next: (sess) => {
          // save locally
          try {
            const exp = sess?.expiresAt ?? (Date.now() + (sess?.ttlMs ?? ttl));
            localStorage.setItem('otpSessionToken', sess.token);
            localStorage.setItem('otpSessionExpiry', String(exp));
          } catch {}
          this.ref.close(<OtpDialogResult>{ remember24h: true, session: sess });
        },
        error: () => {
          // Session fehlgeschlagen -> Code zurückgeben
          this.ref.close(<OtpDialogResult>{ code: this.code, remember24h: false });
        },
        complete: () => (this.pending = false),
      });
      return;
    }

    // Normale Bestätigung (nur Code)
    this.ref.close(<OtpDialogResult>{ code: this.code, remember24h: false });
  }

  // Fallbacks nur fürs Template
  getTitleText(): string { return this.dialogTitle ?? this.cfg?.dialogTitle ?? 'Confirm with OTP'; }
  getHintText(): string | undefined { return this.dialogHint ?? this.cfg?.dialogHint; }
}
