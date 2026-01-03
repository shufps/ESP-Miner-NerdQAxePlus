import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbIconModule, NbAlertModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { SystemComponent } from './system.component';
import { PipesModule } from '../../pipes/pipes.module';
import { ScrollingModule } from '@angular/cdk/scrolling';

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
    NbIconModule,
    NbAlertModule,
    TranslateModule,
    PipesModule,
    ScrollingModule,
  ],
  exports: [
    SystemComponent
  ]
})
export class SystemModule { }
