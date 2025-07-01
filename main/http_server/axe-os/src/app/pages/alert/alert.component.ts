import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { NbToastrService } from '@nebular/theme';
import { catchError, of } from 'rxjs';
import { HttpErrorResponse } from '@angular/common/http';

import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { IAlertSettings } from '../../models/IAlertSettings';

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
    private loadingService: LoadingService
  ) {}

  ngOnInit(): void {
    this.systemService.getAlertInfo(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(data => {
        this.form = this.fb.group({
          discordURL: [data.discordURL, [
            Validators.required,
            Validators.pattern(/^https:\/\/discord\.com\/api\/webhooks\/.+$/)
          ]],
          alertEnable: [data.alertEnable === 1]
        });
      });
  }

  public save() {
    const form = this.form.getRawValue();

    const payload: IAlertSettings = {
      discordURL: form.discordURL,
      alertEnable: form.alertEnable
    };

    this.systemService.updateAlertInfo(this.uri, payload)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastrService.success('Saved alert settings.', 'Success');
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger('Could not save alert settings.', err.message);
        }
      });
  }

  public restart() {
    this.systemService.restart()
      .pipe(catchError(error => {
        this.toastrService.danger('Restart failed.', 'Error');
        return of(null);
      }))
      .subscribe(res => {
        if (res !== null) {
          this.toastrService.success('Device restarted.', 'Success');
        }
      });
  }

  public sendTest() {
    this.systemService.sendAlertTest(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastrService.success('Test alert sent to Discord.', 'Success');
        },
        error: () => {
          this.toastrService.danger('Failed to send test alert.', 'Error');
        }
      });
  }

}
