import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule } from '@nebular/theme';
import { LogsComponent } from './logs.component';
import { PipesModule} from '../../pipes/pipes.module';

@NgModule({
  declarations: [
    LogsComponent,
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
    LogsComponent
  ]
})
export class LogsModule { }
