import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { Component, OnInit, OnDestroy } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
//import { ToastrService } from 'ngx-toastr';
//import { FileUploadHandlerEvent } from 'primeng/fileupload';
import { map, Observable, catchError, of, shareReplay, startWith, tap, switchMap, Subscription } from 'rxjs';
import { GithubUpdateService, UpdateStatus, VersionComparison } from '../../services/github-update.service';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';

@Component({
  selector: 'app-settings',
  templateUrl: './settings.component.html',
  styleUrls: ['./settings.component.scss']
})
export class SettingsComponent implements OnInit, OnDestroy {

  public form!: FormGroup;

  public firmwareUpdateProgress: number | null = 0;
  public websiteUpdateProgress: number | null = 0;

  public deviceModel: string = "";
  public devToolsOpen: boolean = false;
  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  public checkLatestRelease: boolean = true; // Auto-check enabled by default
  public latestRelease$: Observable<any>;
  public expectedFileName: string = "";

  public selectedFirmwareFile: File | null = null;
  public selectedWebsiteFile: File | null = null;

  public info$: Observable<any>;

  public isWebsiteUploading = false;
  public isFirmwareUploading = false;

  // New properties for enhanced update system
  public updateStatus: UpdateStatus = UpdateStatus.UNKNOWN;
  public UpdateStatus = UpdateStatus; // Make enum available in template
  public versionComparison: VersionComparison | null = null;
  public showChangelog: boolean = false;
  public changelog: string = '';
  public currentVersion: string = '';
  public latestRelease: any = null;

  // Enhanced progress tracking
  public updateStep: 'idle' | 'downloading' | 'uploading' | 'flashing' | 'complete' = 'idle';
  public downloadProgress: number = 0;
  public isDirectUpdateInProgress: boolean = false;
  public updateStatusMessage: string = '';
  public otaProgress: number = 0;
  private wsSubscription?: Subscription;
  private rebootCheckInterval?: any;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private githubUpdateService: GithubUpdateService,
    private translate: TranslateService
  ) {
    window.addEventListener('resize', this.checkDevTools);
    this.checkDevTools();

    this.latestRelease$ = this.githubUpdateService.getReleases().pipe(
      map(releases => releases[0]),
      tap(release => {
        this.latestRelease = release;
        this.updateVersionStatus();
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.info$ = this.systemService.getInfo(0).pipe(shareReplay({ refCount: true, bufferSize: 1 }));
  }

  ngOnInit() {
    this.info$.pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.currentVersion = info.version;
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

        // Replace 'γ' with 'Gamma' if present and remove spaces
        // Keep special characters like + as GitHub releases use them
        const normalizedModel = this.deviceModel
          .replace(/γ/g, 'Gamma')
          .replace(/\s+/g, '');     // Remove spaces only

        this.expectedFileName = `esp-miner-${normalizedModel}.bin`;

        console.log('Device model from API:', this.deviceModel);
        console.log('Expected filename:', this.expectedFileName);

        // Update version status after we have both current version and latest release
        this.updateVersionStatus();
      });
  }

  ngOnDestroy() {
    // Clear reboot check interval
    if (this.rebootCheckInterval) {
      clearInterval(this.rebootCheckInterval);
    }
  }

  /**
   * Start checking if device has rebooted and is back online
   */
  private startRebootCheck() {
    // Wait 5 seconds before starting to check (give device time to actually reboot)
    setTimeout(() => {
      let attemptCount = 0;
      const maxAttempts = 60; // Try for 60 seconds

      this.rebootCheckInterval = setInterval(() => {
        attemptCount++;

        // Try to fetch system info
        this.systemService.getInfo(0).subscribe({
          next: (info) => {
            // Device is back online!
            clearInterval(this.rebootCheckInterval);
            this.updateStatusMessage = 'Redémarrage terminé, rechargement de la page...';

            // Reload page after a short delay
            setTimeout(() => {
              window.location.reload();
            }, 2000);
          },
          error: (err) => {
            // Device not ready yet, keep trying
            this.updateStatusMessage = `Redémarrage en cours... (${attemptCount}/${maxAttempts})`;

            if (attemptCount >= maxAttempts) {
              clearInterval(this.rebootCheckInterval);
              this.updateStatusMessage = 'Le redémarrage prend plus de temps que prévu. Veuillez rafraîchir manuellement.';
              this.isFirmwareUploading = false;
            }
          }
        });
      }, 1000); // Check every second
    }, 5000); // Wait 5 seconds before starting
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
          this.toastrService.success(this.translate.instant('TOAST.SAVED'), this.translate.instant('TOAST.SUCCESS'));
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger(`${this.translate.instant('TOAST.COULD_NOT_SAVE')}. ${err.message}`, this.translate.instant('TOAST.ERROR'));
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
      this.toastrService.warning(this.translate.instant('TOAST.NO_FILE_SELECTED'), this.translate.instant('TOAST.WARNING'));
      return;
    }

    if (this.selectedFirmwareFile.name !== this.expectedFileName) {
      this.toastrService.danger(`${this.translate.instant('TOAST.INCORRECT_FILE')}: ${this.expectedFileName}`, this.translate.instant('TOAST.ERROR'));
      return;
    }

    this.isFirmwareUploading = true;

    this.systemService.performOTAUpdate(this.selectedFirmwareFile)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: (event) => {
          if (event.type === HttpEventType.UploadProgress && event.total) {
            console.log(event.loaded);
            console.log(event.total);
            this.firmwareUpdateProgress = Math.round(100 * event.loaded / event.total);
          } else if (event.type === HttpEventType.Response) {
            this.firmwareUpdateProgress = 100;
            this.toastrService.success(this.translate.instant('TOAST.FIRMWARE_UPDATED'), this.translate.instant('TOAST.SUCCESS'));
          }
        },
        error: (err) => {
          this.toastrService.danger(`${this.translate.instant('TOAST.UPLOAD_FAILED')}: ${err.message}`, this.translate.instant('TOAST.ERROR'));
          this.isFirmwareUploading = false;
          this.firmwareUpdateProgress = 0;
        },
        complete: () => {
          this.isFirmwareUploading = false;
          // Optionally reset the progress indicator after a short delay
          setTimeout(() => this.firmwareUpdateProgress = 0, 500);
        }
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
      this.toastrService.warning(this.translate.instant('TOAST.NO_FILE_SELECTED'), this.translate.instant('TOAST.WARNING'));
      return;
    }

    if (this.selectedWebsiteFile.name !== 'www.bin') {
      this.toastrService.danger(`${this.translate.instant('TOAST.INCORRECT_FILE')}: www.bin`, this.translate.instant('TOAST.ERROR'));
      return;
    }

    this.isWebsiteUploading = true;

    this.systemService.performWWWOTAUpdate(this.selectedWebsiteFile)
    .pipe(this.loadingService.lockUIUntilComplete())
    .subscribe({
      next: (event) => {
        if (!event) {
          return; // Skip processing if the event is undefined.
        }
        if (event.type === HttpEventType.UploadProgress && event.total) {
          this.websiteUpdateProgress = Math.round(100 * event.loaded / event.total);
        } else if (event.type === HttpEventType.Response) {
          this.websiteUpdateProgress = 100;
          this.toastrService.success(this.translate.instant('TOAST.WEBSITE_UPDATED'), this.translate.instant('TOAST.SUCCESS'));
          setTimeout(() => window.location.reload(), 1000);
        }
      },
      error: (err) => {
        this.toastrService.danger(`${this.translate.instant('TOAST.UPLOAD_FAILED')}: ${err.message}`, this.translate.instant('TOAST.ERROR'));
        this.isWebsiteUploading = false;
        this.websiteUpdateProgress = 0;
      },
      complete: () => {
        this.isWebsiteUploading = false;
        setTimeout(() => this.websiteUpdateProgress = 0, 500);
      }
    });


    this.selectedWebsiteFile = null;
  }


  /**
   * Update version status based on current and latest versions
   */
  private updateVersionStatus() {
    if (this.currentVersion && this.latestRelease) {
      this.updateStatus = this.githubUpdateService.getUpdateStatus(this.currentVersion, this.latestRelease);
      this.versionComparison = this.githubUpdateService.getVersionComparison(this.currentVersion, this.latestRelease);
    }
  }

  /**
   * Get status badge color based on update status
   */
  public getStatusBadgeColor(): string {
    switch (this.updateStatus) {
      case UpdateStatus.UP_TO_DATE:
        return 'success';
      case UpdateStatus.UPDATE_AVAILABLE:
        return 'warning';
      case UpdateStatus.OUTDATED:
        return 'danger';
      default:
        return 'basic';
    }
  }

  /**
   * Get translation key for status badge
   * Converts 'up-to-date' to 'UPDATE.STATUS_UP_TO_DATE'
   */
  public getStatusTranslationKey(): string {
    const statusKey = this.updateStatus.toUpperCase().replace(/-/g, '_');
    return `UPDATE.STATUS_${statusKey}`;
  }

  /**
   * Toggle changelog visibility
   */
  public toggleChangelog() {
    if (!this.showChangelog && this.latestRelease) {
      this.changelog = this.githubUpdateService.getChangelog(this.latestRelease);
    }
    this.showChangelog = !this.showChangelog;
  }

  /**
   * Direct update from GitHub via backend proxy
   */
  public directUpdateFromGithub(updateType: 'firmware' | 'website') {
    if (!this.latestRelease) {
      this.toastrService.warning(this.translate.instant('TOAST.NO_RELEASE_INFO'), this.translate.instant('TOAST.WARNING'));
      return;
    }

    const filename = updateType === 'firmware' ? this.expectedFileName : 'www.bin';
    console.log('Looking for file:', filename);
    console.log('Device model:', this.deviceModel);
    console.log('Available assets:', this.latestRelease.assets?.map(a => a.name));

    let asset = this.githubUpdateService.findAsset(this.latestRelease, filename);

    // Fallback: try to find by partial match if exact match fails
    if (!asset && updateType === 'firmware') {
      // Normalize for comparison: keep alphanumeric and + character
      const normalizedModel = this.deviceModel.toLowerCase().replace(/[^a-z0-9+]/g, '');
      asset = this.latestRelease.assets?.find(a => {
        const normalizedAsset = a.name.toLowerCase().replace(/[^a-z0-9+]/g, '');
        // Exclude factory versions and www.bin
        return normalizedAsset.includes(normalizedModel)
          && a.name.endsWith('.bin')
          && !a.name.includes('www')
          && !a.name.includes('factory');
      });

      if (asset) {
        console.log('Found asset by partial match:', asset.name);
      }
    }

    if (!asset) {
      const availableFiles = this.latestRelease.assets?.map(a => a.name).join(', ') || 'none';
      this.toastrService.danger(
        `File "${filename}" not found. Available files: ${availableFiles}`,
        'Error',
        { duration: 10000 }
      );
      return;
    }

    // Reset OTA progress
    this.otaProgress = 0;

    if (updateType === 'firmware') {
      this.isFirmwareUploading = true;
      this.firmwareUpdateProgress = 0;
    } else {
      this.isWebsiteUploading = true;
      this.websiteUpdateProgress = 0;
    }

    const otaType = updateType === 'firmware' ? 'firmware' : 'www';
    const assetUrl = asset.browser_download_url;

    // Both firmware and www.bin now use backend streaming (no CORS issues, no PSRAM buffer)
    console.log(`${otaType} OTA from: ${assetUrl}`);

    this.systemService.performGithubOTAUpdate(assetUrl, otaType)
      .subscribe({
        next: () => {
          if (otaType === 'firmware') {
            this.toastrService.success(this.translate.instant('TOAST.FIRMWARE_UPDATED'), this.translate.instant('TOAST.SUCCESS'));
          } else {
            this.toastrService.success(this.translate.instant('TOAST.WEBSITE_UPDATED'), this.translate.instant('TOAST.SUCCESS'));
            setTimeout(() => window.location.reload(), 2000);
          }
        },
        error: (err) => {
          // Check if error is due to GitHub CDN URL length limitation
          if (err.error && err.error.includes('GitHub CDN URL too long')) {
            const errorMsg = otaType === 'www'
              ? 'Les URLs GitHub CDN sont trop longues pour ESP32. Veuillez télécharger www.bin manuellement depuis GitHub et uploader via l\'interface web.'
              : 'URL GitHub trop longue pour ESP32';
            this.toastrService.danger(errorMsg, this.translate.instant('TOAST.ERROR'), { duration: 10000 });
          } else {
            this.toastrService.danger(`${this.translate.instant('TOAST.UPDATE_FAILED')}: ${err.message || err.error}`, this.translate.instant('TOAST.ERROR'));
          }
          this.resetUpdateState(updateType);
        }
      });
  }

  /**
   * Get download URL for direct link
   */
  public getDownloadUrl(updateType: 'firmware' | 'website'): string | null {
    if (!this.latestRelease) {
      return null;
    }

    const filename = updateType === 'firmware' ? this.expectedFileName : 'www.bin';
    let asset = this.githubUpdateService.findAsset(this.latestRelease, filename);

    // Fallback: try to find by partial match if exact match fails
    if (!asset && updateType === 'firmware') {
      // Normalize for comparison: keep alphanumeric and + character
      const normalizedModel = this.deviceModel.toLowerCase().replace(/[^a-z0-9+]/g, '');
      asset = this.latestRelease.assets?.find(a => {
        const normalizedAsset = a.name.toLowerCase().replace(/[^a-z0-9+]/g, '');
        // Exclude factory versions and www.bin
        return normalizedAsset.includes(normalizedModel)
          && a.name.endsWith('.bin')
          && !a.name.includes('www')
          && !a.name.includes('factory');
      });
    }

    return asset ? asset.browser_download_url : null;
  }

  /**
   * Reset update state after completion or error
   */
  private resetUpdateState(updateType: 'firmware' | 'website') {
    this.isDirectUpdateInProgress = false;
    this.updateStep = 'idle';
    this.downloadProgress = 0;

    if (updateType === 'firmware') {
      this.isFirmwareUploading = false;
      this.firmwareUpdateProgress = 0;
    } else {
      this.isWebsiteUploading = false;
      this.websiteUpdateProgress = 0;
    }
  }

  /**
   * Get current update step label
   */
  public getUpdateStepLabel(): string {
    switch (this.updateStep) {
      case 'downloading':
        return 'Downloading from GitHub...';
      case 'uploading':
        return 'Uploading to device...';
      case 'flashing':
        return 'Flashing...';
      case 'complete':
        return 'Complete!';
      default:
        return '';
    }
  }

  /**
   * Get filtered assets (only matching firmware and www.bin)
   */
  public getFilteredAssets(): any[] {
    if (!this.latestRelease?.assets) {
      return [];
    }

    const normalizedModel = this.deviceModel.toLowerCase().replace(/[^a-z0-9+]/g, '');

    return this.latestRelease.assets.filter(asset => {
      // Always include www.bin
      if (asset.name === 'www.bin') {
        return true;
      }

      // Include matching firmware (excluding factory versions)
      const normalizedAsset = asset.name.toLowerCase().replace(/[^a-z0-9+]/g, '');
      return normalizedAsset.includes(normalizedModel)
        && asset.name.endsWith('.bin')
        && !asset.name.includes('factory');
    });
  }

  public restart() {
    this.systemService.restart().pipe(
      catchError(error => {
        this.toastrService.danger(this.translate.instant('TOAST.RESTART_FAILED'), this.translate.instant('TOAST.ERROR'));
        return of(null);
      })
    ).subscribe(res => {
      if (res !== null) {
        this.toastrService.success(this.translate.instant('TOAST.DEVICE_RESTARTED'), this.translate.instant('TOAST.SUCCESS'));
      }
    });
  }





}
