import { CommonModule } from '@angular/common';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { NbAlertModule, NbButtonModule, NbCheckboxModule } from '@nebular/theme';
import { AlertBannerComponent } from './alert-banner.component';

@NgModule({
  declarations: [
    AlertBannerComponent,
    // ...
  ],
  imports: [
    FormsModule,
    NbAlertModule, NbButtonModule, NbCheckboxModule, CommonModule
    // ...
  ],
  exports: [AlertBannerComponent]
})
export class AlertBannerModule {}
