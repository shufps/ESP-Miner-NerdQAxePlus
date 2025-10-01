import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { LanguageSelectorComponent } from './container/language-selector/language-selector.component';
import { StoreModule } from '@ngrx/store';
import { NbSelectModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';

import * as fromI18n from './reducers';

@NgModule({
  imports: [
    CommonModule,
    NbSelectModule,
    TranslateModule,
    StoreModule.forFeature(fromI18n.i18nFeatureKey, fromI18n.reducers)
  ],
  declarations: [LanguageSelectorComponent],
  exports: [LanguageSelectorComponent, TranslateModule]
})
export class I18nModule {}
