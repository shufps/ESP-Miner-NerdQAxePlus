import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { tap, catchError, of, switchMap } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { NbToastrService, NbDialogService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';

@Component({
  selector: 'app-influx',
  templateUrl: './influxdb.component.html',
  styleUrls: ['./influxdb.component.scss']
})
export class InfluxdbComponent implements OnInit {

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
    this.systemService.getInfluxInfo(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.form = this.fb.group({
          influxURL: [info.influxURL, [
            Validators.required,
            Validators.pattern(/^http:\/\/.*[^:]*$/), // http:// without port
          ]],
          influxPort: [info.influxPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          influxToken: ['password', [Validators.required]],
          influxBucket: [info.influxBucket, [Validators.required]],
          influxOrg: [info.influxOrg, [Validators.required]],
          influxPrefix: [info.influxPrefix, [Validators.required]],
          influxEnable: [info.influxEnable == 1]
        });
      });
  }

  public updateSystem() {
    const form = this.form.getRawValue();

    const payload: any = {
      influxURL: form.influxURL,
      influxPort: form.influxPort,
      influxBucket: form.influxBucket,
      influxOrg: form.influxOrg,
      influxPrefix: form.influxPrefix,
      influxEnable: !!form.influxEnable,
    };
    if (form.influxToken !== 'password') {
      payload.influxToken = form.influxToken;
    }

    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.updateInflux(this.uri, payload, totp)
            .pipe(this.loadingService.lockUIUntilComplete())
        )
      )
      .subscribe({
        next: () => {
          this.toastrService.success(this.translate.instant('INFLUXDB.SETTINGS_SAVED'), this.translate.instant('COMMON.SUCCESS'));
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger(this.translate.instant('INFLUXDB.SETTINGS_SAVE_FAILED'), `${this.translate.instant('COMMON.ERROR')}. ${err.message}`);
        }
      });
  }

  public restart() {
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.restart("", totp).pipe(
            // drop session on reboot
            tap(() => {}),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
          console.log(err);
          this.toastrService.danger(this.translate.instant('SYSTEM.RESTART_FAILED'), this.translate.instant('COMMON.ERROR'));
          return of(null);
        })
      )
      .subscribe(res => {
        if (res !== null) {
          this.toastrService.success(this.translate.instant('SYSTEM.RESTART_SUCCESS'), this.translate.instant('COMMON.SUCCESS'));
        }
      });
  }
}
