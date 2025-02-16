import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule } from '@angular/forms';
import { NbCardModule, NbIconModule, NbButtonModule, NbInputModule, NbSelectModule, NbCheckboxModule, NbTabsetModule/*, NbSliderModule*/ } from '@nebular/theme';
import { EditComponent } from './edit.component';
import { AdvancedToggleModule } from '../../components/advanced-toggle/advanced-toggle.module';

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
  ],
  exports: [EditComponent]
})
export class EditModule { }
