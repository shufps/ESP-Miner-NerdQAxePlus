import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbCheckboxModule, NbInputModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { AlertComponent } from './alert.component';

@NgModule({
  declarations: [
    AlertComponent
  ],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    FormsModule,
    NbCardModule,
    NbButtonModule,
    NbCheckboxModule,
    NbInputModule,
    TranslateModule
  ],
  exports: [
    AlertComponent
  ]
})
export class AlertModule { }
