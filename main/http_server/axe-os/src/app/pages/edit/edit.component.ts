import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, TemplateRef } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { switchMap, forkJoin, startWith, tap, catchError, of } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService, NbDialogService, NbDialogRef } from '@nebular/theme';
import { LocalStorageService } from 'src/app/services/local-storage.service';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { TranslateService } from '@ngx-translate/core';
import { IStratum } from 'src/app/models/IStratum';

enum SupportLevel { Safe = 0, Advanced = 1, Pro = 2 }

@Component({
  selector: 'app-edit',
  templateUrl: './edit.component.html',
  styleUrls: ['./edit.component.scss']
})
export class EditComponent implements OnInit {
  public supportLevel: SupportLevel = SupportLevel.Safe;

  public form!: FormGroup;

  public dialogRef!: NbDialogRef<any>; // Store reference

  public frequencyOptions: { name: string; value: number }[] = [];
  public voltageOptions: { name: string; value: number }[] = [];

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;

  public dontShowWarning: boolean = false;

  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  public defaultFrequency: number = 0;
  public defaultCoreVoltage: number = 0;
  public defaultVrFrequency: number = 0;

  public ecoFrequency: number = 0;
  public ecoCoreVoltage: number = 0;

  private originalSettings!: any;

  public otpEnabled = false;
  private pendingTotp: string | undefined;

  private asicFrequencyValues: number[] = [];
  private asicVoltageValues: number[] = [];

  private stratum : IStratum = null;

  private rebootRequiredFields = new Set<string>([
    'flipscreen',
    'invertscreen',
    'hostname',
    'ssid',
    'wifiPass',
    'wifiStatus',
    'invertfanpolarity',
    'stratumDifficulty',
    'stratum_keep',
    'poolMode',
  ]);

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private localStorageService: LocalStorageService,
    private dialogService: NbDialogService,
    private otpAuth: OtpAuthService,
    private translate: TranslateService,
  ) { }

  ngOnInit(): void {
    forkJoin({
      info: this.systemService.getInfo(0, this.uri),
      asic: this.systemService.getAsicInfo(this.uri)
    })
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(({ info, asic }) => {
        this.originalSettings = structuredClone(info);

        // nasty work around
        this.originalSettings["poolMode"] = info.stratum?.poolMode ?? 0;

        this.otpEnabled = !!info.otp;

        // Model still from /info (enum-typed)
        this.ASICModel = info.ASICModel;

        // Prefer defaults from /asic, otherwise fallback to /info
        this.defaultFrequency = (asic?.defaultFrequency ?? info.defaultFrequency ?? 0);
        this.defaultCoreVoltage = (asic?.defaultVoltage ?? info.defaultCoreVoltage ?? 0);

        // eco only from /asic (optional)
        this.ecoFrequency = asic?.ecoFrequency ?? undefined;
        this.ecoCoreVoltage = asic?.ecoVoltage ?? undefined;

        // Store raw options (can be empty if the endpoint returns nothing)
        this.asicFrequencyValues = asic?.frequencyOptions ?? [];
        this.asicVoltageValues = asic?.voltageOptions ?? [];

        this.defaultVrFrequency = info.defaultVrFrequency ?? undefined;

        const freqBase = this.asicFrequencyValues.map(v => {
          let suffix = '';
          if (v === this.defaultFrequency) suffix = ' (default)';
          if (this.ecoFrequency != null && v === this.ecoFrequency) suffix = ' (eco)';
          return { name: `${v}${suffix}`, value: v };
        });

        const voltBase = this.asicVoltageValues.map(v => {
          let suffix = '';
          if (v === this.defaultCoreVoltage) suffix = ' (default)';
          if (this.ecoCoreVoltage != null && v === this.ecoCoreVoltage) suffix = ' (eco)';
          return { name: `${v}${suffix}`, value: v };
        });

        // Build dropdowns and, if needed, append the current custom value
        this.frequencyOptions = this.assembleDropdownOptions(freqBase, info.frequency);
        this.voltageOptions = this.assembleDropdownOptions(voltBase, info.coreVoltage);

        // fix setting where we allowed to disable temp shutdown
        if (info.overheat_temp == 0) {
          info.overheat_temp = 70;
        }
        // respect bounds
        info.overheat_temp = Math.max(40, Math.min(90, info.overheat_temp));

        // Build the form (Min/Max for volt/freq will be set dynamically right after)
        this.form = this.fb.group({
          stratum_keep: [info.stratum_keep == 1],
          flipscreen: [info.flipscreen == 1],
          invertscreen: [info.invertscreen == 1],
          autoscreenoff: [info.autoscreenoff == 1],
          timeFormat: [this.localStorageService.getItem('timeFormat') || '24h'],
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
          stratumEnonceSubscribe: [info.stratumEnonceSubscribe == 1],

          fallbackStratumURL: [info.fallbackStratumURL, [
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          fallbackStratumPort: [info.fallbackStratumPort, [
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          fallbackStratumUser: [info.fallbackStratumUser],
          fallbackStratumPassword: ['*****'],
          fallbackStratumEnonceSubscribe: [info.fallbackStratumEnonceSubscribe == 1],

          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],

          coreVoltage: [info.coreVoltage, [Validators.min(1005), Validators.max(1400), Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          jobInterval: [info.jobInterval, [Validators.required]],
          stratumDifficulty: [info.stratumDifficulty, [Validators.required, Validators.min(1)]],

          poolMode: [info.stratum?.poolMode ?? 0, [Validators.required]],        // 0 = Failover, 1 = Dual
          poolBalance: [info.stratum?.poolBalance ?? 50, [                  // Anteil PRIMARY in %
            Validators.required,
            Validators.min(0),
            Validators.max(100),
          ]],

          autofanspeed: [info.autofanspeed ?? 0, [Validators.required]],
          pidTargetTemp: [info.pidTargetTemp ?? 55, [
            Validators.min(30),
            Validators.max(80),
            Validators.required
          ]],
          pidP: [info.pidP ?? 6, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          pidI: [info.pidI ?? 0.1, [
            Validators.min(0),
            Validators.max(10),
            Validators.required
          ]],
          pidD: [info.pidD ?? 10, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          invertfanpolarity: [info.invertfanpolarity == 1, [Validators.required]],
          manualFanSpeed: [info.manualFanSpeed, [Validators.required]],
          overheat_temp: [info.overheat_temp, [
            Validators.min(40),
            Validators.max(90),
            Validators.required
          ]],
          vrFrequency: [info.vrFrequency, [
            Validators.min(1000),
            Validators.max(100000),
            Validators.pattern(/^\d+$/),   // only ints
            Validators.required,
          ]],
          otpEnabled: [info.otp],
        });

        this.stratum = info.stratum;

        this.form.controls['autofanspeed'].valueChanges
          .pipe(startWith(this.form.controls['autofanspeed'].value))
          .subscribe(() => this.updatePIDFieldStates());

        this.updatePIDFieldStates();
      });
  }

  private updatePIDFieldStates(): void {
    const mode = this.form.controls['autofanspeed'].value;
    const enable = (ctrl: string) => this.form.controls[ctrl]?.enable({ emitEvent: false });
    const disable = (ctrl: string) => this.form.controls[ctrl]?.disable({ emitEvent: false });

    if (mode === 0) {
      enable('manualFanSpeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 1) {
      disable('manualFanSpeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 2) {
      disable('manualFanSpeed');
      enable('pidTargetTemp');
      if (this.supportLevel >= 1) {
        enable('pidP');
        enable('pidI');
        enable('pidD');
      } else {
        disable('pidP');
        disable('pidI');
        disable('pidD');
      }
    }
  }

  public updateSystem(totp?: string) {
    const form = this.form.getRawValue();

    // Client-only preference
    if (form.timeFormat) {
      this.localStorageService.setItem('timeFormat', form.timeFormat);
      window.dispatchEvent(new CustomEvent('timeFormatChanged', { detail: form.timeFormat }));
      delete form.timeFormat;
    }

    // Allow empty WiFi password; strip masked fields
    form.wifiPass = form.wifiPass == null ? '' : form.wifiPass;
    if (form.wifiPass === '*****') delete form.wifiPass;
    if (form.stratumPassword === '*****') delete form.stratumPassword;
    if (form.fallbackStratumPassword === '*****') delete form.fallbackStratumPassword;

    form.stratum_keep = form.stratum_keep ? 1 : 0;

    if (this.pendingTotp) {
      form.totp = this.pendingTotp;
    }

    return this.systemService.updateSystem(this.uri, form, totp)
  }

  get requiresReboot(): boolean {
    if (!this.form || !this.originalSettings) return false;

    const current = this.form.getRawValue();

    for (const key of this.rebootRequiredFields) {
      if (!(key in current)) {
        continue;
      }

      const currentValue = this.normalizeValue(current[key]);
      const originalValue = this.normalizeValue(this.originalSettings[key]);

      // Special case: masked password fields
      if (typeof currentValue === 'string' && currentValue === '*****') {
        continue; // User hasn't changed this field
      }

      if (currentValue !== originalValue) {
        //console.log(`Mismatch on key: ${key}`, currentValue, originalValue);
        return true;
      }
    }

    return false;
  }

  private normalizeValue(value: any): any {
    if (typeof value === 'boolean') {
      return value ? 1 : 0;
    }
    return value;
  }

  showStratumPassword: boolean = false;
  toggleStratumPasswordVisibility() {
    this.showStratumPassword = !this.showStratumPassword;
  }

  showFallbackStratumPassword: boolean = false;
  toggleFallbackStratumPasswordVisibility() {
    this.showFallbackStratumPassword = !this.showFallbackStratumPassword;
  }

  showWifiPassword: boolean = false;
  toggleWifiPasswordVisibility() {
    this.showWifiPassword = !this.showWifiPassword;
  }

  public setDevToolsOpen(supportLevel: number) {
    this.supportLevel = supportLevel;
    console.log('Advanced Mode:', supportLevel);

    const freqBase = this.asicFrequencyValues.map(v => {
      let suffix = '';
      if (v === this.defaultFrequency) suffix = ' (default)';
      if (this.ecoFrequency != null && v === this.ecoFrequency) suffix = ' (eco)';
      return { name: `${v}${suffix}`, value: v };
    });

    const voltBase = this.asicVoltageValues.map(v => {
      let suffix = '';
      if (v === this.defaultCoreVoltage) suffix = ' (default)';
      if (this.ecoCoreVoltage != null && v === this.ecoCoreVoltage) suffix = ' (eco)';
      return { name: `${v}${suffix}`, value: v };
    });

    this.frequencyOptions = this.assembleDropdownOptions(freqBase, this.form.controls['frequency'].value);
    this.voltageOptions = this.assembleDropdownOptions(voltBase, this.form.controls['coreVoltage'].value);

    this.updatePIDFieldStates();
  }

  public isVoltageTooHigh(): boolean {
    if (!this.asicVoltageValues.length) return false;
    const maxVoltage = Math.max(...this.asicVoltageValues);
    return this.form?.controls['coreVoltage'].value > maxVoltage;
  }

  public isFrequencyTooHigh(): boolean {
    if (!this.asicFrequencyValues.length) return false;
    const maxFrequency = Math.max(...this.asicFrequencyValues);
    return this.form?.controls['frequency'].value > maxFrequency;
  }

  public checkVoltageLimit(): void {
    this.form.controls['coreVoltage'].updateValueAndValidity({ emitEvent: false });
  }

  public checkFrequencyLimit(): void {
    this.form.controls['frequency'].updateValueAndValidity({ emitEvent: false });
  }

  /**
   * Dynamically assemble dropdown options, including custom values.
   * @param predefined The predefined options.
   * @param currentValue The current value to include as a custom option if needed.
   */
  private assembleDropdownOptions(predefined: { name: string, value: number }[], currentValue: number): { name: string, value: number }[] {
    const options = [...predefined];
    if (!options.some(option => option.value === currentValue)) {
      options.push({
        name: `${currentValue} (custom)`,
        value: currentValue
      });
    }
    return options;
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
            tap(() => this.otpAuth.clearSession()),
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

  // Function to check if settings are unsafe
  public hasUnsafeSettings(): boolean {
    return this.isVoltageTooHigh() || this.isFrequencyTooHigh();
  }

  // Open warning modal unless user disabled it
  public confirmSave(dialog: TemplateRef<any>): void {
    if (!this.localStorageService.getBool('hideUnsafeSettingsWarning') && this.hasUnsafeSettings()) {
      this.dialogRef = this.dialogService.open(dialog, { closeOnBackdropClick: false });
    } else {
      this.runSaveWithOptionalOtp();
    }
  }

  // Save preference and close modal
  public saveAfterWarning(): void {
    if (this.dontShowWarning) {
      this.localStorageService.setBool('hideUnsafeSettingsWarning', true);
    }
    this.dialogRef.close();
    this.runSaveWithOptionalOtp();
  }

  get wrapAroundTime(): number {
    const freq = this.form.get('vrFrequency')?.value;
    if (!freq || freq <= 0) {
      return 0;
    }
    const wrap = 65536 / freq; // seconds
    return wrap;
  }

  private runSaveWithOptionalOtp(): void {
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.updateSystem(totp).pipe(this.loadingService.lockUIUntilComplete())
        ),
      )
      .subscribe({
        next: () => {
          this.toastrService.success('Success!', 'Saved.');
        },
        error: (err: HttpErrorResponse) => {
          this.toastrService.danger('Error.', `Could not save. ${err.message}`);
        }
      });
  }

  public poolTabHeader(i: 0 | 1) {
    if (this.form?.get("poolMode")?.value == 0) {
      if (i == 0) {
        return this.translate.instant('SETTINGS.PRIMARY_STRATUM_POOL');
      }
      return this.translate.instant('SETTINGS.FALLBACK_POOL');
    }
    return `Pool ${i + 1}`;
  }
}

