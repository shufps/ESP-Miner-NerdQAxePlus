import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { NbToastrService, NbDialogService } from '@nebular/theme';
import { HttpErrorResponse } from '@angular/common/http';
import { EMPTY, of } from 'rxjs';
import { catchError, switchMap, take } from 'rxjs/operators';

import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { TranslateService } from '@ngx-translate/core';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';

const WEBHOOK_URL_PATTERN = /^https?:\/\/.+$/i;

@Component({
  selector: 'app-alert',
  templateUrl: './alert.component.html',
  styleUrls: ['./alert.component.scss']
})
export class AlertComponent implements OnInit {

  public form!: FormGroup;
  public hasWebhook = false;
  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private translate: TranslateService,
    private otpAuth: OtpAuthService,
  ) { }

  ngOnInit(): void {
    this.systemService.getAlertInfo(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe((data: any) => {
        this.hasWebhook = !!data?.hasWebhook;
        this.form = this.fb.group({
          // Per-topic toggles
          watchdogEnable: [data?.watchdogEnable === 1],
          blockFoundEnable: [data?.blockFoundEnable === 1],
          bestDiffEnable: [data?.bestDiffEnable === 1],
          coinbaseVerifyEnable: [data?.coinbaseVerifyEnable === 1],
          showBlockFoundScreen: [data?.showBlockFoundScreen === 1],

          webhookUrl: [data?.hasWebhook ? 'WEBHOOK' : '', [
            Validators.required,
            Validators.pattern(WEBHOOK_URL_PATTERN)
          ]],
        });
      });
  }

  public saveAlerts() {
    const form = this.form.getRawValue();
    const payload: any = {
      watchdogEnable: !!form.watchdogEnable,
      blockFoundEnable: !!form.blockFoundEnable,
      bestDiffEnable: !!form.bestDiffEnable,
      coinbaseVerifyEnable: !!form.coinbaseVerifyEnable,
    };
    if (form.webhookUrl !== 'WEBHOOK') {
      payload.webhookUrl = form.webhookUrl || '';
    }
    this.savePatch(payload, () => {
      this.hasWebhook = !!payload.webhookUrl;
      this.form.controls['webhookUrl'].setValue(this.hasWebhook ? 'WEBHOOK' : '');
    });
  }

  public saveDisplay() {
    const form = this.form.getRawValue();
    this.savePatch({ showBlockFoundScreen: !!form.showBlockFoundScreen });
  }

  private savePatch(payload: any, onSuccess?: () => void) {
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.updateAlertInfo(this.uri, payload, totp)
            .pipe(this.loadingService.lockUIUntilComplete())
        ),
      )
      .subscribe({
        next: () => {
          onSuccess?.();
          this.toastrService.success(this.translate.instant('ALERTS.SETTINGS_SAVED'), this.translate.instant('COMMON.SUCCESS'));
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger(this.translate.instant('ALERTS.SETTINGS_SAVE_FAILED'), err.message);
        }
      });
  }

  public sendTest() {
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.sendAlertTest(this.uri, totp)
            .pipe(this.loadingService.lockUIUntilComplete())
        ),
      )
      .subscribe({
        next: () => {
          this.toastrService.success(this.translate.instant('ALERTS.TEST_ALERT_SENT'), this.translate.instant('COMMON.SUCCESS'));
        },
        error: () => {
          this.toastrService.danger(this.translate.instant('ALERTS.TEST_ALERT_FAILED'), this.translate.instant('COMMON.ERROR'));
        }
      });
  }
}
