import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NbButtonModule, NbIconModule, NbTooltipModule } from '@nebular/theme';
import { AdvancedToggleComponent } from './advanced-toggle.component';

@NgModule({
  declarations: [AdvancedToggleComponent],
  imports: [
    CommonModule,
    NbButtonModule,
    NbIconModule,
    NbTooltipModule
  ],
  exports: [AdvancedToggleComponent] // Export so it can be used elsewhere
})
export class AdvancedToggleModule {}
