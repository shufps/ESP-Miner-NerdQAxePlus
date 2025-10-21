import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { Component, OnInit, OnDestroy } from '@angular/core';
import { FormBuilder, FormGroup, Validators, FormControl } from '@angular/forms';
import { BehaviorSubject, combineLatest, map, Observable, catchError, of, shareReplay, startWith, Subscription, interval } from 'rxjs';
import { switchMap, takeWhile, tap, take } from 'rxjs/operators';
import { GithubUpdateService, UpdateStatus, VersionComparison, GithubRelease } from '../../services/github-update.service';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { IUpdateStatus } from 'src/app/models/IUpdateStatus';
import { NbDialogService } from '@nebular/theme';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';

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
  public expectedFactoryFilename: string = "";

  public selectedFirmwareFile: File | null = null;
  public selectedWebsiteFile: File | null = null;

  public info$: Observable<any>;

  public isWebsiteUploading = false;
  public isFirmwareUploading = false;
  public isOneClickUpdate = false;

  private updateStatusSub?: Subscription;
  private sawRebooting = false;

  public currentStep: string = "";

  // New properties for enhanced update system
  public updateStatus: UpdateStatus = UpdateStatus.UNKNOWN;
  public UpdateStatus = UpdateStatus; // Make enum available in template
  public versionComparison: VersionComparison | null = null;
  public showChangelog: boolean = false;
  public changelog: string = '';
  public currentVersion: string = '';
  public latestRelease: any = null;

  public otpEnabled: boolean = false;

  // Enhanced progress tracking
  public updateStep: 'idle' | 'downloading' | 'uploading' | 'flashing' | 'complete' = 'idle';
  public downloadProgress: number = 0;
  public isDirectUpdateInProgress: boolean = false;
  public updateStatusMessage: string = '';
  public otaProgress: number = 0;
  private wsSubscription?: Subscription;
  private rebootCheckInterval?: any;

  private normalizedModel: string = '';

  public includePrereleasesCtrl = new FormControl<boolean>(false);
  public releases$!: Observable<GithubRelease[]>;   // list shown in dropdown
  public selectedRelease: GithubRelease | null = null;
  private latestStableRelease: GithubRelease | null = null;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private githubUpdateService: GithubUpdateService,
    private dialog: NbDialogService,
    private translate: TranslateService,
    private otpAuth: OtpAuthService,
  ) {
    window.addEventListener('resize', this.checkDevTools);
    this.checkDevTools();

    // Stream: always keep a "stable latest" for status badge etc.
    const latestStable$ = this.githubUpdateService.getReleases(false).pipe(
      map((rels) => rels?.[0] ?? null),
      tap((rel) => {
        this.latestRelease = rel; // BEHÄLT deine alte Variable für Status
        this.latestStableRelease = rel;
        this.updateVersionStatus();
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    // Stream: releases for dropdown depending on the checkbox
    this.releases$ = this.includePrereleasesCtrl.valueChanges.pipe(
      startWith(this.includePrereleasesCtrl.value),
      switchMap((include) => this.githubUpdateService.getReleases(include)),
      tap((list) => {
        // Auto-select first entry if nothing selected or selection no longer present
        if (!this.selectedRelease || !list.find(r => r.id === this.selectedRelease!.id)) {
          this.selectedRelease = list[0] ?? null;
          this.updateSelectedReleaseDeps(); // update filenames/changelog
        }
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.latestRelease$ = latestStable$;

    this.info$ = this.systemService.getInfo(0).pipe(
      shareReplay({ refCount: true, bufferSize: 1 })
    );
  }

  ngOnInit() {
    this.info$.pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.currentVersion = info.version;
        this.deviceModel = "NerdQAxe++";
        //this.deviceModel = info.deviceModel;
        this.ASICModel = info.ASICModel;
        this.otpEnabled = !!info.otp;

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
        this.normalizedModel = this.normalizeModel(this.deviceModel)
        this.expectedFileName = `esp-miner-${this.normalizedModel}.bin`;

        console.log('Device model from API:', this.deviceModel);
        console.log('Expected filename:', this.expectedFileName);

        // Update version status after we have both current version and latest release
        this.updateVersionStatus();
      });

    // Build releases$ AFTER info$ is available, and filter by asset existence
    this.releases$ = combineLatest([
      this.includePrereleasesCtrl.valueChanges.pipe(startWith(this.includePrereleasesCtrl.value)),
      this.info$ // ensures deviceModel is loaded first
    ]).pipe(
      switchMap(([include]) =>
        this.githubUpdateService.getReleases(include).pipe(
          map(list =>
            (list ?? []).filter(r =>
              r.assets?.some(a => a.name === this.buildFactoryNameFor(r))
            )
          )
        )
      ),
      tap(list => {
        // Auto-select first valid release if current selection missing
        if (!this.selectedRelease || !list.find(r => r.id === this.selectedRelease!.id)) {
          this.selectedRelease = list[0] ?? null;
          this.updateSelectedReleaseDeps();
        }
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.checkUpdateStatus();
  }

  private normalizeModel(model) {
    return model.replace(/γ/g, 'Gamma').replace(/\s+/g, '');
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
            this.updateStatusMessage = 'Reboot complete, reloading page...';

            // Reload page after a short delay
            setTimeout(() => {
              window.location.reload();
            }, 2000);
          },
          error: (err) => {
            // Device not ready yet, keep trying
            this.updateStatusMessage = `Reboot in progress... (${attemptCount}/${maxAttempts})`;

            if (attemptCount >= maxAttempts) {
              clearInterval(this.rebootCheckInterval);
              this.updateStatusMessage = 'The reboot is taking longer than expected. Please refresh manually.';
              this.isOneClickUpdate = false;
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

    const file = this.selectedFirmwareFile;

    this.otpAuth.ensureOtp$(
      "",
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_FW_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) => {
          this.isFirmwareUploading = true;
          return this.systemService.performOTAUpdate(file, totp)
            .pipe(this.loadingService.lockUIUntilComplete());
        })
      )
      .subscribe({
        next: (event) => {
          if (event?.type === HttpEventType.UploadProgress && event.total) {
            this.firmwareUpdateProgress = Math.round(100 * event.loaded / event.total);
          } else if (event?.type === HttpEventType.Response) {
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
    const file = this.selectedWebsiteFile;

    this.otpAuth.ensureOtp$(
      "",
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_FW_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) => {
          this.isWebsiteUploading = true;
          return this.systemService.performWWWOTAUpdate(file, totp)
            .pipe(this.loadingService.lockUIUntilComplete());
        })
      )
      .subscribe({
        next: (event) => {
          if (!event) return;
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
    if (this.currentVersion && this.latestStableRelease) {
      this.updateStatus = this.githubUpdateService.getUpdateStatus(
        this.currentVersion,
        this.latestStableRelease
      );
      this.versionComparison = this.githubUpdateService.getVersionComparison(
        this.currentVersion,
        this.latestStableRelease
      );
    }
    this.updateSelectedReleaseDeps();
  }

  /** Refresh filename + changelog for the selected release */
  private updateSelectedReleaseDeps() {
    if (!this.selectedRelease) {
      this.expectedFactoryFilename = '';
      return;
    }
    this.expectedFactoryFilename = this.buildFactoryNameFor(this.selectedRelease);

    // Refresh changelog if panel is open
    if (this.showChangelog) {
      this.changelog = this.githubUpdateService.getChangelog(this.selectedRelease);
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

  /** Label for dropdown: "vX.Y.Z (latest)" for the newest item */
  public getReleaseLabel(r: GithubRelease, idx: number): string {
    return r.isLatest ? `${r.tag_name} (latest)` : r.tag_name;
  }

  /**
   * Toggle changelog visibility
   */
  public toggleChangelog() {
    this.showChangelog = !this.showChangelog;

    if (this.showChangelog && this.selectedRelease) {
      this.changelog = this.githubUpdateService.getChangelog(this.selectedRelease);
    }
  }

  /**
   * Direct update from GitHub via backend proxy
   */
  public directUpdateFromGithub() {
    if (!this.selectedRelease) {
      this.toastrService.warning(this.translate.instant('TOAST.NO_RELEASE_INFO'), this.translate.instant('TOAST.WARNING'));
      return;
    }

    const filename = this.expectedFactoryFilename;
    console.log('Looking for file:', filename);
    console.log('Device model:', this.deviceModel);
    //console.log('Available assets:', this.selectedRelease?.assets?.map(a => a.name) ?? []);
    const asset = this.githubUpdateService.findAsset(this.selectedRelease, filename);
    if (!asset) {
      this.toastrService.danger(`File "${filename}" not found.`, 'Error', { duration: 10000 });
      return;
    }

    const assetUrl = asset.browser_download_url;

    this.otpAuth.ensureOtp$(
      "",
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_FW_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) => {
          // reset UI states
          this.otaProgress = 0;
          this.isOneClickUpdate = true;
          this.firmwareUpdateProgress = 0;

          // kick the backend update
          return this.systemService.performGithubOTAUpdate(assetUrl, totp);
        })
      )
      .subscribe({
        next: () => {
          this.startUpdatePolling();
        },
        error: (err) => {
          this.toastrService.danger(`${this.translate.instant('TOAST.UPDATE_FAILED')}: ${err.message || err.error}`, this.translate.instant('TOAST.ERROR'));
          this.isOneClickUpdate = false;
        }
      });
  }

  private startUpdatePolling() {
    this.stopUpdatePolling();
    this.sawRebooting = false;

    this.updateStatusSub = interval(1000)
      .pipe(
        // Poll OTA status every second
        switchMap(() => this.systemService.getGithubOTAStatus()),
        tap((status: IUpdateStatus) => {
          // Update UI state
          this.otaProgress = status.progress;
          this.currentStep = `UPDATE.STEP_${status.step.toUpperCase()}`;
          //console.log('Update status:', status);

          // check if device finished updating and only fire the success toast a single time
          if (status.step === 'rebooting' && !this.sawRebooting) {
            this.sawRebooting = true;
            this.toastrService.success(this.translate.instant('TOAST.FIRMWARE_UPDATED'), this.translate.instant('TOAST.SUCCESS'));
            this.startRebootCheck();
          }
        })
      )
      .subscribe({
        error: (err) => {
          // ignore errors
        }
      });
  }

  // we can resume the update progress status on a page reload because
  // the OTA update is not done in HTTP server context anymore! 😍
  private checkUpdateStatus() {
    // Single-shot status fetch
    this.systemService.getGithubOTAStatus()
      .pipe(take(1))
      .subscribe({
        next: (status: IUpdateStatus) => {
          // If update is ongoing, (re)start polling
          if (status.pending || status.running) {
            this.isOneClickUpdate = true;
            this.otaProgress = status.progress;
            this.currentStep = `UPDATE.STEP_${status.step.toUpperCase()}`;
            this.startUpdatePolling();
          }
        },
      });
  }

  private stopUpdatePolling() {
    if (this.updateStatusSub) {
      this.updateStatusSub.unsubscribe();
      this.updateStatusSub = undefined;
    }
  }

  /**
   * Get filtered assets (only matching factory firmware)
   */
  public getFilteredAssets(): any[] {
    return this.latestRelease.assets.filter(asset => {
      return asset.name === this.expectedFactoryFilename;
    });
  }

  public restart() {
    this.otpAuth.ensureOtp$(
      "",
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.restart("", totp).pipe(
            // drop session on reboot
            tap(() => { }),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
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

  // settings.component.ts
  public onSelectReleaseId(id: number) {
    this.releases$.pipe(take(1)).subscribe(list => {
      const sel = list.find(r => r.id === id);
      if (sel) {
        this.selectedRelease = sel;
        this.updateSelectedReleaseDeps();
      }
    });
  }

  // settings.component.ts
  public trackRelease = (_: number, r: GithubRelease) => r.id;

  // Helper to build expected factory filename for a given release
  private buildFactoryNameFor(release: GithubRelease): string {
    return `esp-miner-factory-${this.normalizedModel}-${release.tag_name}.bin`;
  }


}
