import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { NbToastrService } from '@nebular/theme';
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
