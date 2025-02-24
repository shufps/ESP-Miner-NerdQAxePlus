import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
//import { ToastrService } from 'ngx-toastr';
import { catchError, of } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { NbToastrService } from '@nebular/theme';

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
    private loadingService: LoadingService
  ) {

  }
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

    if (form.influxToken === 'password') {
      delete form.influxToken;
    }

    this.systemService.updateInflux(this.uri, form)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastrService.success('Success!', 'Saved.');
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger('Error.', `Could not save. ${err.message}`);
        }
      });
  }

  public restart() {
    this.systemService.restart().pipe(
      catchError(error => {
        this.toastrService.danger(`Failed to restart Device`, 'Error');
        return of(null);
      })
    ).subscribe(res => {
      if (res !== null) {
        this.toastrService.success(`Device restarted`, 'Success');
      }
    });
  }

}
