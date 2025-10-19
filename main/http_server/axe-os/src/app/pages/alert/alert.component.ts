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

@Component({
  selector: 'app-alert',
  templateUrl: './alert.component.html',
  styleUrls: ['./alert.component.scss']
})
export class AlertComponent implements OnInit {

  public form!: FormGroup;
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
        this.form = this.fb.group({
          // Per-topic toggles
          alertDiscordWatchdogEnable: [data?.alertDiscordWatchdogEnable === 1],
          alertDiscordBlockFoundEnable: [data?.alertDiscordBlockFoundEnable === 1],

          // Keep sentinel so users must enter a valid webhook at least once.
          alertDiscordWebhook: ['WEBHOOK', [
            Validators.required,
            Validators.pattern(/^https:\/\/discord\.com\/api\/webhooks\/.+$/)
          ]],
        });
      });
  }

  public save() {
    const form = this.form.getRawValue();

    // Build payload; strip sentinel if user didn’t update the webhook.
    const payload: any = {
      alertDiscordWatchdogEnable: !!form.alertDiscordWatchdogEnable,
      alertDiscordBlockFoundEnable: !!form.alertDiscordBlockFoundEnable,
    };

    if (form.alertDiscordWebhook !== 'WEBHOOK') {
      payload.alertDiscordWebhook = form.alertDiscordWebhook;
    }

    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_FW_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.updateInflux(this.uri, payload, totp)
            .pipe(this.loadingService.lockUIUntilComplete())
        ),
      )
      .subscribe({
        next: () => {
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
      this.translate.instant('SECURITY.OTP_FW_HINT')
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
