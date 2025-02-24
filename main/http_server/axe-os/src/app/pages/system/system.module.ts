import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule } from '@nebular/theme';
import { SystemComponent } from './system.component';
import { PipesModule} from '../../pipes/pipes.module';

@NgModule({
  declarations: [
    SystemComponent,
  ],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    FormsModule,
    NbCardModule,
    NbButtonModule,
    PipesModule,
  ],
  exports: [
    SystemComponent
  ]
})
export class SystemModule { }
