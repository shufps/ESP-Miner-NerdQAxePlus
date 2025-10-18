import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { catchError, of, switchMap, take, EMPTY } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { NbToastrService, NbDialogService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { OtpDialogComponent } from '../../components/otp-dialog/otp-dialog.component';

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
    private dialog: NbDialogService,
  ) {}

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

  /** If OTP is enabled on the device, prompt for a code and return it; otherwise return undefined. */
  private requireTotpIfEnabled(title: string, hint: string) {
    return this.systemService.getInfo(0, this.uri).pipe(
      take(1),
      switchMap(info => {
        if (!info?.otp) return of(undefined); // no OTP required
        const ref = this.dialog.open(OtpDialogComponent, {
          closeOnBackdropClick: false,
          context: { title, hint, periodSec: 30 },
        });
        return ref.onClose.pipe(
          take(1),
          switchMap(code => code ? of(code as string) : EMPTY) // cancel -> abort chain
        );
      })
    );
  }

  public updateSystem() {
    const form = this.form.getRawValue();

    // Don't overwrite token if user left placeholder
    if (form.influxToken === 'password') {
      delete form.influxToken;
    }

    this.requireTotpIfEnabled(
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_FW_HINT')
    )
    .pipe(
      switchMap(code =>
        this.systemService.updateInflux(this.uri, form, code) 
          .pipe(this.loadingService.lockUIUntilComplete())
      ),
      catchError((err: HttpErrorResponse) => {
        // Falls Backend 401 wegen OTP ablehnt
        this.toastrService.danger(
          this.translate.instant('INFLUXDB.SETTINGS_SAVE_FAILED'),
          `${this.translate.instant('COMMON.ERROR')}. ${err.message}`
        );
        return of(null);
      })
    )
    .subscribe(res => {
      if (res !== null) {
        this.toastrService.success(
          this.translate.instant('INFLUXDB.SETTINGS_SAVED'),
          this.translate.instant('COMMON.SUCCESS')
        );
      }
    });
  }

  public restart() {
    this.systemService.restart().pipe(
      catchError(error => {
        this.toastrService.danger(this.translate.instant('SYSTEM.RESTART_FAILED'), this.translate.instant('COMMON.ERROR'));
        return of(null);
      })
    ).subscribe(res => {
      if (res !== null) {
        this.toastrService.success(this.translate.instant('SYSTEM.RESTART_SUCCESS'), this.translate.instant('COMMON.SUCCESS'));
      }
    });
  }
}
