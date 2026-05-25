import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, TemplateRef } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { switchMap, startWith, tap, catchError, of } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService, NbDialogService, NbDialogRef } from '@nebular/theme';
import { LocalStorageService } from 'src/app/services/local-storage.service';
import { OtpAuthService, EnsureOtpResult, EnsureOtpOptions } from '../../services/otp-auth.service';
import { TranslateService } from '@ngx-translate/core';
import { ISettingsV2, ISettingsV2Fan } from '../../models/ISettingsV2';

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
  public asicModel!: eASICModel;

  public defaultFrequency: number = 0;
  public defaultCoreVoltage: number = 0;
  public defaultVrFrequency: number = 0;
  public fanCount: number = 1;

  public lastCoinbaseVerifyMode: number = 1;
  public lastFallbackCoinbaseVerifyMode: number = 1;

  toggleCoinbaseVerify(enabled: boolean, controlName: string, lastRef: 'lastCoinbaseVerifyMode' | 'lastFallbackCoinbaseVerifyMode') {
    const ctrl = this.form.controls[controlName];
    if (enabled) {
      ctrl.setValue(this[lastRef]);
    } else {
      this[lastRef] = ctrl.value;
      ctrl.setValue(0);
    }
  }
  public fanLabels: string[] = ['Fan 1', 'Fan 2'];

  public ecoFrequency: number = 0;
  public ecoCoreVoltage: number = 0;

  private originalSettings!: any;

  public otpEnabled = false;
  public hasCanExtension = false;
  private pendingTotp: string | undefined;

  private asicFrequencyValues: number[] = [];
  private asicVoltageValues: number[] = [];

  private rebootRequiredFields = new Set<string>([
    'flipScreen',
    'invertScreen',
    'hostname',
    'ssid',
    'wifiPass',
    'wifiStatus',
    'invertFanPolarity',
    'stratumDifficulty',
    'stratumKeep',
    'canMaster',
    'poolMode',
    'stratumProtocol',
    'fallbackStratumProtocol',
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
    this.systemService.getSettingsV2(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe((info: ISettingsV2) => {
        this.originalSettings = structuredClone(info);

        this.originalSettings["poolMode"] = info.poolMode ?? 0;

        this.otpEnabled = !!info.otp;
        this.hasCanExtension = !!info.can.hasExtension;

        this.asicModel = info.asicModel;

        this.defaultFrequency = info.defaultFrequency ?? 0;
        this.defaultCoreVoltage = info.defaultCoreVoltage ?? 0;

        this.ecoFrequency = info.ecoFrequency ?? undefined;
        this.ecoCoreVoltage = info.ecoCoreVoltage ?? undefined;

        this.asicFrequencyValues = info.frequencyOptions ?? [];
        this.asicVoltageValues = info.voltageOptions ?? [];

        this.defaultVrFrequency = info.defaultVrFrequency ?? undefined;

        this.fanCount = info.fans?.length ?? 1;
        this.fanLabels = info.fans?.map((f: ISettingsV2Fan, i: number) => f.label || `Fan ${i + 1}`) ?? ['Fan 1', 'Fan 2'];
        const fan1cfg = info.fans?.[1];

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

        // Build the form (Min/Max for volt/freq will be set dynamically right after)
        this.form = this.fb.group({
          stratumKeep: [info.stratumKeep == 1],
          canMaster: [info.can.enabled == true],
          flipScreen: [info.flipScreen == 1],
          invertScreen: [info.invertScreen == 1],
          autoScreenOff: [info.autoScreenOff == 1],
          timeFormat: [this.localStorageService.getItem('timeFormat') || '24h'],
          stratumURL: [info.pools[0].url, [
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          stratumPort: [info.pools[0].port, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          stratumUser: [info.pools[0].user, [Validators.required]],
          stratumPassword: ['*****', [Validators.required]],
          stratumEnonceSubscribe: [info.pools[0].enonceSubscribe == 1],
          stratumTLS: [info.pools[0].tls == 1],
          coinbaseVerifyMode: [info.pools[0].coinbaseVerifyMode ?? 0],
          coinbaseMaxFee: [info.pools[0].coinbaseMaxFee ?? 3.0],
          coinbaseVerifyForce: [info.pools[0].coinbaseVerifyForce ?? false],

          fallbackStratumURL: [info.pools[1].url, [
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          fallbackStratumPort: [info.pools[1].port, [
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          fallbackStratumUser: [info.pools[1].user],
          fallbackStratumPassword: ['*****'],
          fallbackStratumEnonceSubscribe: [info.pools[1].enonceSubscribe == 1],
          fallbackStratumTLS: [info.pools[1].tls == 1],
          fallbackCoinbaseVerifyMode: [info.pools[1].coinbaseVerifyMode ?? 0],
          fallbackCoinbaseMaxFee: [info.pools[1].coinbaseMaxFee ?? 3],
          fallbackCoinbaseVerifyForce: [info.pools[1].coinbaseVerifyForce ?? false],

          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],

          coreVoltage: [info.coreVoltage, [Validators.min(1005), Validators.max(1400), Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          jobInterval: [info.jobInterval, [Validators.required]],
          stratumDifficulty: [info.stratumDifficulty, [Validators.required, Validators.min(1)]],

          stratumProtocol: [info.pools[0].protocol ?? 0, [Validators.required]],   // 0 = V1, 1 = V2
          fallbackStratumProtocol: [info.pools[1].protocol ?? 0],
          sv2AuthorityPubkey: [info.pools[0].sv2AuthorityPubkey ?? ''],
          fallbackSv2AuthorityPubkey: [info.pools[1].sv2AuthorityPubkey ?? ''],
          sv2ChannelType: [info.pools[0].sv2ChannelType ?? 0],                      // 0 = Extended, 1 = Standard
          fallbackSv2ChannelType: [info.pools[1].sv2ChannelType ?? 0],

          poolMode: [info.poolMode ?? 0, [Validators.required]],                   // 0 = Failover, 1 = Dual
          poolBalance: [info.poolBalance ?? 50, [                                   // Anteil PRIMARY in %
            Validators.required,
            Validators.min(0),
            Validators.max(100),
          ]],

          autofanspeed: [info.fans[0]?.mode ?? 0, [Validators.required]],
          pidTargetTemp: [info.fans[0]?.pid?.targetTemp ?? 55, [
            Validators.min(30),
            Validators.max(80),
            Validators.required
          ]],
          pidP: [info.fans[0]?.pid?.p ?? 6, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          pidI: [info.fans[0]?.pid?.i ?? 0.1, [
            Validators.min(0),
            Validators.max(10),
            Validators.required
          ]],
          pidD: [info.fans[0]?.pid?.d ?? 10, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          invertFanPolarity: [info.invertFanPolarity == 1, [Validators.required]],
          manualFanSpeed: [info.fans[0]?.manualSpeed ?? 100, [Validators.required]],
          overheat_temp: [info.fans[0]?.overheatTemp ?? 70, [
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

          fan1Mode: [fan1cfg?.mode ?? 3, [Validators.required]],
          fan1ManualSpeed: [fan1cfg?.manualSpeed ?? 100, [Validators.min(0), Validators.max(100), Validators.required]],
          fan1OverheatTemp: [fan1cfg?.overheatTemp ?? 70, [Validators.min(40), Validators.max(90), Validators.required]],
          fan1PidTargetTemp: [fan1cfg?.pid?.targetTemp ?? 65, [Validators.min(30), Validators.max(80), Validators.required]],
          fan1PidP: [fan1cfg?.pid?.p ?? 6, [Validators.min(0), Validators.max(100), Validators.required]],
          fan1PidI: [fan1cfg?.pid?.i ?? 0.1, [Validators.min(0), Validators.max(10), Validators.required]],
          fan1PidD: [fan1cfg?.pid?.d ?? 10, [Validators.min(0), Validators.max(100), Validators.required]],
        });

        this.lastCoinbaseVerifyMode = info.pools[0].coinbaseVerifyMode || 1;
        this.lastFallbackCoinbaseVerifyMode = info.pools[1].coinbaseVerifyMode || 1;

        this.form.controls['autofanspeed'].valueChanges
          .pipe(startWith(this.form.controls['autofanspeed'].value))
          .subscribe(() => this.updatePIDFieldStates());

        this.form.controls['fan1Mode'].valueChanges
          .pipe(startWith(this.form.controls['fan1Mode'].value))
          .subscribe(() => this.updateFan1FieldStates());

        this.updatePIDFieldStates();
        this.updateFan1FieldStates();

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

  private updateFan1FieldStates(): void {
    const mode = this.form.controls['fan1Mode'].value;
    const enable = (ctrl: string) => this.form.controls[ctrl]?.enable({ emitEvent: false });
    const disable = (ctrl: string) => this.form.controls[ctrl]?.disable({ emitEvent: false });

    if (mode === 3) {
      // LINKED — disable fan1-controls; overheatTemp stays enabled (VReg shutdown threshold)
      enable('fan1OverheatTemp');
      disable('fan1ManualSpeed');
      disable('fan1PidTargetTemp');
      disable('fan1PidP');
      disable('fan1PidI');
      disable('fan1PidD');
    } else if (mode === 0) {
      // MANUAL
      enable('fan1ManualSpeed');
      enable('fan1OverheatTemp');
      disable('fan1PidTargetTemp');
      disable('fan1PidP');
      disable('fan1PidI');
      disable('fan1PidD');
    } else if (mode === 2) {
      // PID
      disable('fan1ManualSpeed');
      enable('fan1OverheatTemp');
      enable('fan1PidTargetTemp');
      if (this.supportLevel >= 1) {
        enable('fan1PidP');
        enable('fan1PidI');
        enable('fan1PidD');
      } else {
        disable('fan1PidP');
        disable('fan1PidI');
        disable('fan1PidD');
      }
    }
  }

  public updateSystem(totp?: string) {
    const f = this.form.getRawValue();

    // Client-only preference
    if (f.timeFormat) {
      this.localStorageService.setItem('timeFormat', f.timeFormat);
      window.dispatchEvent(new CustomEvent('timeFormatChanged', { detail: f.timeFormat }));
    }

    // Build pools[] array matching GET /api/v2/settings structure
    const pool0: any = {
      url: f.stratumURL,
      port: f.stratumPort,
      user: f.stratumUser,
      enonceSubscribe: !!f.stratumEnonceSubscribe,
      tls: !!f.stratumTLS,
      protocol: f.stratumProtocol,
      sv2AuthorityPubkey: f.sv2AuthorityPubkey,
      sv2ChannelType: f.sv2ChannelType,
      coinbaseVerifyMode: f.coinbaseVerifyMode,
      coinbaseMaxFee: f.coinbaseMaxFee,
      coinbaseVerifyForce: !!f.coinbaseVerifyForce,
    };
    if (f.stratumPassword !== '*****') pool0.password = f.stratumPassword;

    const pool1: any = {
      url: f.fallbackStratumURL,
      port: f.fallbackStratumPort,
      user: f.fallbackStratumUser,
      enonceSubscribe: !!f.fallbackStratumEnonceSubscribe,
      tls: !!f.fallbackStratumTLS,
      protocol: f.fallbackStratumProtocol,
      sv2AuthorityPubkey: f.fallbackSv2AuthorityPubkey,
      sv2ChannelType: f.fallbackSv2ChannelType,
      coinbaseVerifyMode: f.fallbackCoinbaseVerifyMode,
      coinbaseMaxFee: f.fallbackCoinbaseMaxFee,
      coinbaseVerifyForce: !!f.fallbackCoinbaseVerifyForce,
    };
    if (f.fallbackStratumPassword !== '*****') pool1.password = f.fallbackStratumPassword;

    // Build fans[] array
    const fans: any[] = [
      {
        mode: f.autofanspeed,
        manualSpeed: f.manualFanSpeed,
        overheatTemp: f.overheat_temp,
        pid: { targetTemp: f.pidTargetTemp, p: f.pidP, i: f.pidI, d: f.pidD }
      }
    ];
    if (this.fanCount > 1) {
      fans.push({
        mode: f.fan1Mode,
        manualSpeed: f.fan1ManualSpeed,
        overheatTemp: f.fan1OverheatTemp,
        pid: { targetTemp: f.fan1PidTargetTemp, p: f.fan1PidP, i: f.fan1PidI, d: f.fan1PidD }
      });
    }

    // Build v2 payload
    const payload: any = {
      // Network
      hostname: f.hostname,
      ssid: f.ssid,
      // ASIC
      frequency: f.frequency,
      coreVoltage: f.coreVoltage,
      vrFrequency: f.vrFrequency,
      jobInterval: f.jobInterval,
      stratumDifficulty: f.stratumDifficulty,
      // Stratum
      poolMode: f.poolMode,
      poolBalance: f.poolBalance,
      stratumKeep: f.stratumKeep ? 1 : 0,
      pools: [pool0, pool1],
      // Fans
      fans,
      invertFanPolarity: !!f.invertFanPolarity,
      // Display
      flipScreen: !!f.flipScreen,
      invertScreen: !!f.invertScreen,
      autoScreenOff: !!f.autoScreenOff,
      // CAN
      canMaster: !!f.canMaster,
    };

    // WiFi password — allow empty, strip masked
    const wifiPass = f.wifiPass == null ? '' : f.wifiPass;
    if (wifiPass !== '*****') payload.wifiPass = wifiPass;

    return this.systemService.updateSettingsV2(this.uri, payload, totp);
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
    this.updateFan1FieldStates();
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
      this.translate.instant('SECURITY.OTP_HINT'),
      { disableOtp: true },
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
    const protoKey = i === 0 ? 'stratumProtocol' : 'fallbackStratumProtocol';
    const proto = this.form?.get(protoKey)?.value;
    const protoLabel = proto === 1 ? ' (SV2)' : ' (SV1)';

    if (this.form?.get("poolMode")?.value == 0) {
      if (i == 0) {
        return this.translate.instant('SETTINGS.PRIMARY_STRATUM_POOL') + protoLabel;
      }
      return this.translate.instant('SETTINGS.FALLBACK_STRATUM_POOL') + protoLabel;
    }
    return `Pool ${i + 1}` + protoLabel;
  }

  public swapPools(): void {
    if (!this.form) return;

    const pairs: [string, string][] = [
      ['stratumURL',              'fallbackStratumURL'],
      ['stratumPort',             'fallbackStratumPort'],
      ['stratumUser',             'fallbackStratumUser'],
      ['stratumPassword',         'fallbackStratumPassword'],
      ['stratumTLS',              'fallbackStratumTLS'],
      ['stratumEnonceSubscribe',  'fallbackStratumEnonceSubscribe'],
      ['stratumProtocol',         'fallbackStratumProtocol'],
      ['sv2AuthorityPubkey',      'fallbackSv2AuthorityPubkey'],
      ['sv2ChannelType',          'fallbackSv2ChannelType'],
      ['coinbaseVerifyMode',      'fallbackCoinbaseVerifyMode'],
      ['coinbaseMaxFee',          'fallbackCoinbaseMaxFee'],
      ['coinbaseVerifyForce',     'fallbackCoinbaseVerifyForce'],
    ];

    const get = (k: string) => this.form.get(k)?.value;
    const set = (k: string, v: any) => this.form.get(k)?.setValue(v, { emitEvent: false });

    for (const [ka, kb] of pairs) {
      const tmp = get(ka);
      set(ka, get(kb));
      set(kb, tmp);
    }
  }

}
