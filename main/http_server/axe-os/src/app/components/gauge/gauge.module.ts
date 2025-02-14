import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { GaugeComponent } from './gauge.component';

@NgModule({
  declarations: [GaugeComponent],
  imports: [CommonModule],
  exports: [GaugeComponent],
})
export class GaugeModule {}
