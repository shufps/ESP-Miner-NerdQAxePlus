import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule } from '@angular/forms';
import { NbCardModule, NbIconModule, NbButtonModule, NbInputModule, NbSelectModule, NbCheckboxModule, NbTabsetModule/*, NbSliderModule*/ } from '@nebular/theme';
import { EditComponent } from './edit.component';
import { AdvancedToggleModule } from '../../components/advanced-toggle/advanced-toggle.module';
import { FormsModule } from '@angular/forms';
import { TranslateModule } from '@ngx-translate/core';
import { AlertBannerModule } from 'src/app/components/alert-banner/alert-banner.module';

@NgModule({
  declarations: [EditComponent],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    NbCardModule,
    NbButtonModule,
    NbInputModule,
    NbSelectModule,
    NbCheckboxModule,
    NbTabsetModule,
    NbIconModule,
    AdvancedToggleModule,
    FormsModule,
    TranslateModule,
    AlertBannerModule,
  ],
  exports: [EditComponent]
})
export class EditModule { }
