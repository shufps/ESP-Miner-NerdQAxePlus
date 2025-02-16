import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NbButtonModule } from '@nebular/theme';
import { AdvancedToggleComponent } from './advanced-toggle.component';

@NgModule({
  declarations: [AdvancedToggleComponent],
  imports: [
    CommonModule,
    NbButtonModule // Import Nebular Button Module
  ],
  exports: [AdvancedToggleComponent] // Export so it can be used elsewhere
})
export class AdvancedToggleModule {}
