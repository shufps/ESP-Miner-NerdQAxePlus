import { NgModule } from '@angular/core';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbIconModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
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
    TranslateModule,
    PipesModule,
    NbIconModule,
  ],
  exports: [
    SystemComponent
  ]
})
export class SystemModule { }
