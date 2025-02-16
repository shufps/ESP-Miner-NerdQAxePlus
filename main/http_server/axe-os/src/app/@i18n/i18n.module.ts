import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { LanguageSelectorComponent } from './container/language-selector/language-selector.component';
import { StoreModule } from '@ngrx/store';

import * as fromI18n from './reducers';

@NgModule({
  imports: [
    CommonModule,
    StoreModule.forFeature(fromI18n.i18nFeatureKey, fromI18n.reducers)
  ],
  declarations: [LanguageSelectorComponent],
  exports: [LanguageSelectorComponent]
})
export class I18nModule {}
