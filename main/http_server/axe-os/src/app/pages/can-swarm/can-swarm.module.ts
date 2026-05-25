import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ReactiveFormsModule } from '@angular/forms';
import {
  NbCardModule,
  NbButtonModule,
  NbIconModule,
  NbInputModule,
  NbSelectModule,
  NbCheckboxModule,
} from '@nebular/theme';
import { CanSwarmComponent } from './can-swarm.component';

@NgModule({
  declarations: [
    CanSwarmComponent,
  ],
  imports: [
    CommonModule,
    ReactiveFormsModule,
    NbCardModule,
    NbButtonModule,
    NbIconModule,
    NbInputModule,
    NbSelectModule,
    NbCheckboxModule,
  ],
  exports: [
    CanSwarmComponent,
  ]
})
export class CanSwarmModule { }
