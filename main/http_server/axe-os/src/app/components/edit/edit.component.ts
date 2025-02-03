import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { startWith } from 'rxjs';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemService } from 'src/app/services/system.service';
import { eASICModel } from 'src/models/enum/eASICModel';

@Component({
  selector: 'app-edit',
  templateUrl: './edit.component.html',
  styleUrls: ['./edit.component.scss']
})
export class EditComponent implements OnInit {

  public form!: FormGroup;

  public frequencyOptions: { name: string; value: number }[] = []; // Declare for frequency options
  public voltageOptions: { name: string; value: number }[] = [];  // Declare for voltage options

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;


  public devToolsOpen: boolean = false;
  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastr: ToastrService,
    private toastrService: ToastrService,
    private loadingService: LoadingService
  ) {

    window.addEventListener('resize', this.checkDevTools);
    this.checkDevTools();

  }
  ngOnInit(): void {
    this.systemService.getInfo(0, this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.ASICModel = info.ASICModel;

        // Assemble dropdown options
        this.frequencyOptions = this.assembleDropdownOptions(this.getPredefinedFrequencies(), info.frequency);
        this.voltageOptions = this.assembleDropdownOptions(this.getPredefinedVoltages(), info.coreVoltage);


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
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          fallbackStratumPort: [info.fallbackStratumPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          fallbackStratumUser: [info.fallbackStratumUser, [Validators.required]],
          fallbackStratumPassword: ['*****', [Validators.required]],

          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],
          coreVoltage: [info.coreVoltage, [Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          jobInterval: [info.jobInterval, [Validators.required]],
          autofanspeed: [info.autofanspeed == 1, [Validators.required]],
          invertfanpolarity: [info.invertfanpolarity == 1, [Validators.required]],
          fanspeed: [info.fanspeed, [Validators.required]],
          overheat_temp: [info.overheat_temp, [Validators.required]]
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


  private checkDevTools = () => {
    const previousState = this.devToolsOpen;
    this.devToolsOpen =
      window.outerWidth - window.innerWidth > 160 ||
      window.outerHeight - window.innerHeight > 160;

    // When transitioning from devTools open to closed
    if (!this.devToolsOpen && previousState) {
      // Update dropdown options with free-text values
      this.frequencyOptions = this.assembleDropdownOptions(
        this.getPredefinedFrequencies(),
        this.form.controls['frequency'].value
      );
      this.voltageOptions = this.assembleDropdownOptions(
        this.getPredefinedVoltages(),
        this.form.controls['coreVoltage'].value
      );
    }
  };


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
          this.toastr.success('Success!', 'Saved.');
        },
        error: (err: HttpErrorResponse) => {
          this.toastr.error('Error.', `Could not save. ${err.message}`);
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

}
