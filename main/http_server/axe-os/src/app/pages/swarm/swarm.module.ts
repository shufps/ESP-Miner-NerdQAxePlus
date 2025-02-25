import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { NbCardModule, NbButtonModule, NbIconModule, NbInputModule, NbLayoutModule } from '@nebular/theme';
import { SwarmComponent } from './swarm.component';
import { EditModule } from '../edit/edit.module';
import { PipesModule} from '../../pipes/pipes.module';

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
    EditModule,
    PipesModule
  ],
  exports: [
    SwarmComponent
  ]
})
export class SwarmModule { }