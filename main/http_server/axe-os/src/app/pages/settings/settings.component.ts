import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { Component } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
//import { ToastrService } from 'ngx-toastr';
//import { FileUploadHandlerEvent } from 'primeng/fileupload';
import { map, Observable, catchError, of, shareReplay, startWith } from 'rxjs';
import { GithubUpdateService } from '../../services/github-update.service';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService } from '@nebular/theme';

@Component({
  selector: 'app-settings',
  templateUrl: './settings.component.html',
  styleUrls: ['./settings.component.scss']
})
export class SettingsComponent {

  public form!: FormGroup;

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;

  public deviceModel: string = "";
  public devToolsOpen: boolean = false;
  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  public checkLatestRelease: boolean = false;
  public latestRelease$: Observable<any>;
  public expectedFileName: string = "";

  public selectedFirmwareFile: File | null = null;
  public selectedWebsiteFile: File | null = null;

  public info$: Observable<any>;

  public isWebsiteUploading = false;
  public isFirmwareUploading = false;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private githubUpdateService: GithubUpdateService
  ) {



    window.addEventListener('resize', this.checkDevTools);
    this.checkDevTools();

    this.latestRelease$ = this.githubUpdateService.getReleases().pipe(map(releases => {
      return releases[0];
    }));

    this.info$ = this.systemService.getInfo(0).pipe(shareReplay({ refCount: true, bufferSize: 1 }))


    this.info$.pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.deviceModel = info.deviceModel;
        this.ASICModel = info.ASICModel;
        this.form = this.fb.group({
          flipscreen: [info.flipscreen == 1],
          invertscreen: [info.invertscreen == 1],
          autoscreenoff: [info.autoscreenoff == 0],
          stratumURL: [info.stratumURL, [
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          stratumPort: [info.stratumPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          stratumUser: [info.stratumUser, [Validators.required]],
          stratumPassword: ['*****', [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],
          coreVoltage: [info.coreVoltage, [Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          jobInterval: [info.jobInterval, [Validators.required]],
          autofanspeed: [info.autofanspeed == 1, [Validators.required]],
          invertfanpolarity: [info.invertfanpolarity == 1, [Validators.required]],
          fanspeed: [info.fanspeed, [Validators.required]],
        });

        this.form.controls['autofanspeed'].valueChanges.pipe(
          startWith(this.form.controls['autofanspeed'].value)
        ).subscribe(autofanspeed => {
          if (autofanspeed) {
            this.form.controls['fanspeed'].disable();
          } else {
            this.form.controls['fanspeed'].enable();
          }
        });
        // Replace 'γ' with 'Gamma' if present
        this.expectedFileName = `esp-miner-${this.deviceModel}.bin`.replace('γ', 'Gamma');
      });

  }
  private checkDevTools = () => {
    if (
      window.outerWidth - window.innerWidth > 160 ||
      window.outerHeight - window.innerHeight > 160
    ) {
      this.devToolsOpen = true;
    } else {
      this.devToolsOpen = false;
    }
  };

  public updateSystem() {

    const form = this.form.getRawValue();

    form.frequency = parseInt(form.frequency);
    form.coreVoltage = parseInt(form.coreVoltage);
    form.jobInterval = parseInt(form.jobInterval);

    // bools to ints
    form.flipscreen = form.flipscreen == true ? 1 : 0;
    form.invertscreen = form.invertscreen == true ? 1 : 0;
    form.invertfanpolarity = form.invertfanpolarity == true ? 1 : 0;
    form.autofanspeed = form.autofanspeed == true ? 1 : 0;
    form.autoscreenoff = form.autoscreenoff == true ? 1 : 0;

    // Allow an empty wifi password
    form.wifiPass = form.wifiPass == null ? '' : form.wifiPass;

    if (form.wifiPass === '*****') {
      delete form.wifiPass;
    }
    if (form.stratumPassword === '*****') {
      delete form.stratumPassword;
    }

    if (form.fallbackStratumPassword === '*****') {
      delete form.fallbackStratumPassword;
    }

    this.systemService.updateSystem(undefined, form)
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


  public onFirmwareFileSelected(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      this.selectedFirmwareFile = input.files[0];
    }
  }

  public uploadFirmwareFile() {
    if (!this.selectedFirmwareFile) {
      this.toastrService.warning('No file selected', 'Warning');
      return;
    }

    if (this.selectedFirmwareFile.name !== this.expectedFileName) {
      this.toastrService.danger(`Incorrect file, expected: ${this.expectedFileName}`, 'Error');
      return;
    }

    this.isFirmwareUploading = true;

    this.systemService.performOTAUpdate(this.selectedFirmwareFile)  // ⬅ Pass file directly
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => this.toastrService.success('Firmware updated successfully', 'Success'),
        error: (err) => {
          this.toastrService.danger(`Upload failed: ${err.message}`, 'Error');
          this.isFirmwareUploading = false;
        },
        complete: () => this.isFirmwareUploading = false,
      });

    this.selectedFirmwareFile = null;
  }

  public onWebsiteFileSelected(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      this.selectedWebsiteFile = input.files[0];
    }
  }

  public uploadWebsiteFile() {
    if (!this.selectedWebsiteFile) {
      this.toastrService.warning('No file selected', 'Warning');
      return;
    }

    if (this.selectedWebsiteFile.name !== 'www.bin') {
      this.toastrService.danger('Incorrect file, expected: www.bin', 'Error');
      return;
    }

    this.isWebsiteUploading = true;

    this.systemService.performWWWOTAUpdate(this.selectedWebsiteFile)  // ⬅ Pass file directly
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastrService.success('Website updated successfully', 'Success');
          setTimeout(() => window.location.reload(), 1000);
        },
        error: (err) => {
          this.toastrService.danger(`Upload failed: ${err.message}`, 'Error');
          this.isWebsiteUploading = false
        },
        complete: () => this.isWebsiteUploading = false
      });

    this.selectedWebsiteFile = null;
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
