import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { DateAgoPipe } from './date-ago.pipe';
import { ANSIPipe } from './ansi.pipe';
import { HashSuffixPipe } from './hash-suffix.pipe';
import { HumanReadablePipe } from './human-readable';

@NgModule({
  declarations: [
    DateAgoPipe,
    ANSIPipe,
    HashSuffixPipe,
    HumanReadablePipe
  ],
  imports: [
    CommonModule
  ],
  exports: [
    DateAgoPipe,
    ANSIPipe,
    HashSuffixPipe,
    HumanReadablePipe
  ]
})
export class PipesModule { }
