import { NgModule } from '@angular/core';
import { NbCardModule, NbIconModule, NbButtonModule } from '@nebular/theme';
import { SettingsComponent } from './settings.component';
import { NbThemeModule, NbLayoutModule } from '@nebular/theme';
import { CommonModule } from '@angular/common';
import { EditModule } from '../edit/edit.module';
import { NbSpinnerModule } from '@nebular/theme';
import { NbProgressBarModule } from '@nebular/theme';

@NgModule({
  declarations: [SettingsComponent],
  imports: [
    CommonModule,
    NbCardModule,
    NbLayoutModule,
    NbThemeModule,
    NbIconModule,
    NbButtonModule,
    EditModule,
    NbSpinnerModule,
    NbProgressBarModule,
  ]
})
export class SettingsModule { }
