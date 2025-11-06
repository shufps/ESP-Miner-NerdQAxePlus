import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbCheckboxModule, NbInputModule, NbTooltipModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { InfluxdbComponent } from './influxdb.component';

@NgModule({
  declarations: [
    InfluxdbComponent
  ],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    FormsModule,
    NbCardModule,
    NbButtonModule,
    NbCheckboxModule,
    NbInputModule,
    NbTooltipModule,
    TranslateModule
  ],
  exports: [
    InfluxdbComponent
  ]
})
export class InfluxDBModule { }
