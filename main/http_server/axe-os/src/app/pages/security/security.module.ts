import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbBadgeModule, NbButtonModule, NbCheckboxModule, NbInputModule, NbTooltipModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { SecurityComponent } from './security.component';
import { OtpDialogModule } from '../../components/otp-dialog/otp-dialog.module';

@NgModule({
  declarations: [
    SecurityComponent
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
    TranslateModule,
    NbBadgeModule,
    OtpDialogModule,
  ],
  exports: [
    SecurityComponent
  ]
})
export class SecurityModule { }
