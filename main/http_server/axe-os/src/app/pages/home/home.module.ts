import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { HomeComponent } from './home.component';
import { NbCardModule, NbButtonModule, NbThemeModule, NbIconModule } from '@nebular/theme';
import { SystemService } from '../../services/system.service';
import { Chart } from 'chart.js/auto'; // Ensure Chart.js is properly imported
import { GaugeModule } from '../../components/gauge/gauge.module';

@NgModule({
  declarations: [
    HomeComponent,
  ],
  imports: [
    CommonModule,
    NbCardModule,
    NbButtonModule,
    NbIconModule,
    NbThemeModule,
    GaugeModule
  ],
  providers: [
    SystemService
  ],
  exports: [
    HomeComponent
  ]
})
export class HomeModule {}
