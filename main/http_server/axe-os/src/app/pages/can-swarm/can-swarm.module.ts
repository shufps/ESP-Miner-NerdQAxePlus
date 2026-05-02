import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { NbCardModule, NbButtonModule, NbIconModule } from '@nebular/theme';
import { CanSwarmComponent } from './can-swarm.component';

@NgModule({
  declarations: [
    CanSwarmComponent,
  ],
  imports: [
    CommonModule,
    NbCardModule,
    NbButtonModule,
    NbIconModule,
  ],
  exports: [
    CanSwarmComponent,
  ]
})
export class CanSwarmModule { }
