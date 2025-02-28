import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { DateAgoPipe } from './date-ago.pipe';
import { ANSIPipe } from './ansi.pipe';
import { HashSuffixPipe } from './hash-suffix.pipe';

@NgModule({
  declarations: [
    DateAgoPipe,
    ANSIPipe,
    HashSuffixPipe
  ],
  imports: [
    CommonModule
  ],
  exports: [
    DateAgoPipe,
    ANSIPipe,
    HashSuffixPipe,
  ]
})
export class PipesModule { }
