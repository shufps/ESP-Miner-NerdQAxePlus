import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { NbCardModule, NbButtonModule, NbIconModule, NbInputModule, NbLayoutModule, NbTooltipModule } from '@nebular/theme';
import { SwarmComponent } from './swarm.component';
import { EditModule } from '../edit/edit.module';
import { PipesModule} from '../../pipes/pipes.module';
import { I18nModule } from '../../@i18n/i18n.module';

@NgModule({
  declarations: [
    SwarmComponent,
  ],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    FormsModule,
    NbCardModule,
    NbButtonModule,
    NbIconModule,
    NbInputModule,
    NbLayoutModule,
    NbTooltipModule,
    EditModule,
    PipesModule,
    I18nModule
  ],
  exports: [
    SwarmComponent
  ]
})
export class SwarmModule { }