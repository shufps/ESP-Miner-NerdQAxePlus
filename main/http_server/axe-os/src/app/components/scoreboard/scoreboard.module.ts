import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ScoreboardComponent } from './scoreboard.component';
import { NbCardModule, NbTooltipModule } from '@nebular/theme';
import { PipesModule } from '../../pipes/pipes.module';
import { TranslateModule } from '@ngx-translate/core';

@NgModule({
  declarations: [
    ScoreboardComponent
  ],
  imports: [
    CommonModule,
    NbCardModule,
    NbTooltipModule,
    PipesModule,
    TranslateModule
  ],
  exports: [
    ScoreboardComponent
  ]
})
export class ScoreboardModule { }
