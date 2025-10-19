import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import {
  NbCardModule,
  NbButtonModule,
  NbInputModule,
  NbDialogModule,
  NbCheckboxModule,
} from '@nebular/theme';

import { OtpDialogComponent } from './otp-dialog.component';

@NgModule({
  declarations: [OtpDialogComponent],
  imports: [
    CommonModule,
    FormsModule,
    NbDialogModule.forChild(),
    NbCardModule,
    NbButtonModule,
    NbInputModule,
    NbCheckboxModule,
  ],
  exports: [OtpDialogComponent],
})
export class OtpDialogModule {}
