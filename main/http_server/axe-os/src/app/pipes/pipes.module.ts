import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { DateAgoPipe } from './date-ago.pipe';
import { ANSIPipe } from './ansi.pipe';

@NgModule({
  declarations: [
    DateAgoPipe,
    ANSIPipe
  ],
  imports: [
    CommonModule
  ],
  exports: [
    DateAgoPipe,
    ANSIPipe
  ]
})
export class PipesModule { }
