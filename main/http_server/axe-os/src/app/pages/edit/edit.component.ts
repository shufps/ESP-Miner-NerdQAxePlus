import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, TemplateRef } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
//import { ToastrService } from 'ngx-toastr';
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
        this.ASICModel = info.ASICModel;

        // Assemble dropdown options
        this.frequencyOptions = this.assembleDropdownOptions(this.getPredefinedFrequencies(), info.frequency);
        this.voltageOptions = this.assembleDropdownOptions(this.getPredefinedVoltages(), info.coreVoltage);

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
          autofanspeed: [info.autofanspeed == 1, [Validators.required]],
          invertfanpolarity: [info.invertfanpolarity == 1, [Validators.required]],
          fanspeed: [info.fanspeed, [Validators.required]],
          overheat_temp: [info.overheat_temp, [
            Validators.min(40),
            Validators.max(90),
            Validators.required]]
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
      });
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
    this.frequencyOptions = this.assembleDropdownOptions(this.getPredefinedFrequencies(), this.form.controls['frequency'].value);
    this.voltageOptions = this.assembleDropdownOptions(this.getPredefinedVoltages(), this.form.controls['coreVoltage'].value);
  }

  public isVoltageTooHigh(): boolean {
    const maxVoltage = Math.max(...this.getPredefinedVoltages().map(v => v.value));
    return this.form?.controls['coreVoltage'].value > maxVoltage;
  }

  public isFrequencyTooHigh(): boolean {
    const maxFrequency = Math.max(...this.getPredefinedFrequencies().map(f => f.value));
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
  private getPredefinedFrequencies(): { name: string, value: number }[] {
    switch (this.ASICModel) {
      case eASICModel.BM1366:
        return [
          { name: '400', value: 400 },
          { name: '425', value: 425 },
          { name: '450', value: 450 },
          { name: '475', value: 475 },
          { name: '485 (default)', value: 485 },
          { name: '500', value: 500 },
          { name: '525', value: 525 },
          { name: '550', value: 550 },
          { name: '575', value: 575 }
        ];
      case eASICModel.BM1368:
        return [
          { name: '400', value: 400 },
          { name: '425', value: 425 },
          { name: '450', value: 450 },
          { name: '475', value: 475 },
          { name: '490 (default)', value: 490 },
          { name: '500', value: 500 },
          { name: '525', value: 525 },
          { name: '550', value: 550 },
          { name: '575', value: 575 }
        ];
      case eASICModel.BM1370:
        return [
          { name: '500', value: 500 },
          { name: '525', value: 525 },
          { name: '550', value: 550 },
          { name: '575', value: 575 },
          { name: '590', value: 590 },
          { name: '600 (default)', value: 600 }
        ];
      default:
        return [];
    }
  }

  /**
   * Returns predefined core voltages based on the current ASIC model.
   */
  private getPredefinedVoltages(): { name: string, value: number }[] {
    switch (this.ASICModel) {
      case eASICModel.BM1366:
        return [
          { name: '1100', value: 1100 },
          { name: '1150', value: 1150 },
          { name: '1200 (default)', value: 1200 },
          { name: '1250', value: 1250 },
          { name: '1300', value: 1300 }
        ];
      case eASICModel.BM1368:
        return [
          { name: '1100', value: 1100 },
          { name: '1150', value: 1150 },
          { name: '1200', value: 1200 },
          { name: '1250 (default)', value: 1250 },
          { name: '1300', value: 1300 },
          { name: '1350', value: 1350 }
        ];
      case eASICModel.BM1370:
        return [
          { name: '1120', value: 1120 },
          { name: '1130', value: 1130 },
          { name: '1140', value: 1140 },
          { name: '1150 (default)', value: 1150 },
          { name: '1160', value: 1160 },
          { name: '1170', value: 1170 },
          { name: '1180', value: 1180 },
          { name: '1190', value: 1190 },
          { name: '1200', value: 1200 },
        ];
      default:
        return [];
    }
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
