import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, TemplateRef } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { startWith, catchError, of } from 'rxjs';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService, NbDialogService, NbDialogRef } from '@nebular/theme';
import { LocalStorageService } from 'src/app/services/local-storage.service';

@Component({
  selector: 'app-edit',
  templateUrl: './edit.component.html',
  styleUrls: ['./edit.component.scss']
})
export class EditComponent implements OnInit {

  public form!: FormGroup;

  public dialogRef!: NbDialogRef<any>; // Store reference

  public frequencyOptions: { name: string; value: number }[] = []; // Declare for frequency options
  public voltageOptions: { name: string; value: number }[] = [];  // Declare for voltage options

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;

  public dontShowWarning: boolean = false; // Track checkbox state

  public devToolsOpen: boolean = false;
  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  public defaultFrequency: number = 0;
  public defaultCoreVoltage: number = 0;

  private originalSettings!: any;

  private rebootRequiredFields = new Set<string>([
    'flipscreen',
    'invertscreen',
    'autoscreenoff',
    'hostname',
    'ssid',
    'wifiPass',
    'wifiStatus',
    'stratumURL',
    'stratumPort',
    'stratumUser',
    'fallbackStratumURL',
    'fallbackStratumPort',
    'fallbackStratumUser',
    'invertfanpolarity',
    'autofanpolarity',
    'stratumDifficulty',
  ]);

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private localStorageService: LocalStorageService,
    private dialogService: NbDialogService
  ) { }

  ngOnInit(): void {
    this.systemService.getInfo(0, this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.originalSettings = structuredClone(info);

        this.ASICModel = info.ASICModel;

        this.defaultFrequency = info.defaultFrequency ?? 0;
        this.defaultCoreVoltage = info.defaultCoreVoltage ?? 0;

        // Assemble dropdown options
        this.frequencyOptions = this.assembleDropdownOptions(this.getPredefinedFrequencies(this.defaultFrequency), info.frequency);
        this.voltageOptions = this.assembleDropdownOptions(this.getPredefinedVoltages(this.defaultCoreVoltage), info.coreVoltage);

        // fix setting where we allowed to disable temp shutdown
        if (info.overheat_temp == 0) {
          info.overheat_temp = 70;
        }

        // respect the new bounds
        info.overheat_temp = Math.max(info.overheat_temp, 40);
        info.overheat_temp = Math.min(info.overheat_temp, 90);

        this.form = this.fb.group({
          flipscreen: [info.flipscreen == 1],
          invertscreen: [info.invertscreen == 1],
          autoscreenoff: [info.autoscreenoff == 1],
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

          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],
          coreVoltage: [info.coreVoltage, [Validators.min(1005), Validators.max(1400), Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          jobInterval: [info.jobInterval, [Validators.required]],
          stratumDifficulty: [info.stratumDifficulty, [Validators.required, Validators.min(1)]],
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
          autofanpolarity: [info.autofanpolarity == 1, [Validators.required]],
          fanspeed: [info.fanspeed, [Validators.required]],
          overheat_temp: [info.overheat_temp, [
            Validators.min(40),
            Validators.max(90),
            Validators.required]]
        });

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
      enable('fanspeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 1) {
      disable('fanspeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 2) {
      disable('fanspeed');
      enable('pidTargetTemp');
      if (this.devToolsOpen) {
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

  public updateSystem() {
    const form = this.form.getRawValue();

    // Allow an empty wifi password
    form.wifiPass = form.wifiPass == null ? '' : form.wifiPass;

    if (form.wifiPass === '*****') {
      delete form.wifiPass;
    }
    if (form.stratumPassword === '*****') {
      delete form.stratumPassword;
    }

    this.systemService.updateSystem(this.uri, form)
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
        console.log(`Mismatch on key: ${key}`, currentValue, originalValue);
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

  public setDevToolsOpen(state: boolean) {
    this.devToolsOpen = state;
    console.log('Advanced Mode:', state); // Debugging output
    this.frequencyOptions = this.assembleDropdownOptions(this.getPredefinedFrequencies(this.defaultFrequency), this.form.controls['frequency'].value);
    this.voltageOptions = this.assembleDropdownOptions(this.getPredefinedVoltages(this.defaultCoreVoltage), this.form.controls['coreVoltage'].value);
    this.updatePIDFieldStates();
  }

  public isVoltageTooHigh(): boolean {
    const maxVoltage = Math.max(...this.getPredefinedVoltages(this.defaultCoreVoltage).map(v => v.value));
    return this.form?.controls['coreVoltage'].value > maxVoltage;
  }

  public isFrequencyTooHigh(): boolean {
    const maxFrequency = Math.max(...this.getPredefinedFrequencies(this.defaultFrequency).map(f => f.value));
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
    // Clone predefined options to avoid side effects
    const options = [...predefined];

    // Add custom value if not already in the list
    if (!options.some(option => option.value === currentValue)) {
      options.push({
        name: `${currentValue} (custom)`,
        value: currentValue
      });
    }

    return options;
  }

  /**
   * Returns predefined frequencies based on the current ASIC model.
   */
  private getPredefinedFrequencies(defaultValue: number): { name: string, value: number }[] {
    let values: number[] = [];
    switch (this.ASICModel) {
      case eASICModel.BM1366:
        values = [400, 425, 450, 475, 485, 500, 525, 550, 575];
        break;
      case eASICModel.BM1368:
        values = [400, 425, 450, 475, 490, 500, 525, 550, 575];
        break;
      case eASICModel.BM1370:
        values = [500, 515, 525, 550, 575, 590, 600];
        break;
      default:
        return [];
    }
    return values.map(val => ({
      name: val === defaultValue ? `${val} (default)` : `${val}`,
      value: val
    }));
  }

  /**
   * Returns predefined core voltages based on the current ASIC model.
   */
  private getPredefinedVoltages(defaultValue: number): { name: string, value: number }[] {
    let values: number[] = [];
    switch (this.ASICModel) {
      case eASICModel.BM1366:
        values = [1100, 1150, 1200, 1250, 1300];
        break;
      case eASICModel.BM1368:
        values = [1100, 1150, 1200, 1250, 1300, 1350];
        break;
      case eASICModel.BM1370:
        values = [1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200];
        break;
      default:
        return [];
    }
    return values.map(val => ({
      name: val === defaultValue ? `${val} (default)` : `${val}`,
      value: val
    }));
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

  // Function to check if settings are unsafe
  public hasUnsafeSettings(): boolean {
    return this.isVoltageTooHigh() || this.isFrequencyTooHigh();
  }

  // Open warning modal unless user disabled it
  public confirmSave(dialog: TemplateRef<any>): void {
    if (!this.localStorageService.getBool('hideUnsafeSettingsWarning') && this.hasUnsafeSettings()) {
      this.dialogRef = this.dialogService.open(dialog, { closeOnBackdropClick: false });
    } else {
      this.updateSystem(); // Directly save if warning is disabled
    }
  }

  // Save preference and close modal
  public saveAfterWarning(): void {
    if (this.dontShowWarning) {
      this.localStorageService.setBool('hideUnsafeSettingsWarning', true); // Save user preference
    }
    this.dialogRef.close();
    this.updateSystem(); // Proceed with saving
  }

}
