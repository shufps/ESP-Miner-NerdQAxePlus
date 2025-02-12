import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbCheckboxModule, NbInputModule } from '@nebular/theme';
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
    NbInputModule
  ],
  exports: [
    InfluxdbComponent
  ]
})
export class InfluxDBModule { }
