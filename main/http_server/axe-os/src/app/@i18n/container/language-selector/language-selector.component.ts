import { Component, OnInit } from '@angular/core';
import { Store } from '@ngrx/store';
import * as fromI18n from '../../reducers';
import { LanguageActions } from '../../actions';
import { Language } from '../../models/language.model';

@Component({
  selector: 'app-language-selector',
  templateUrl: './language-selector.component.html'
})
export class LanguageSelectorComponent implements OnInit {
  constructor(private readonly store: Store<fromI18n.State>) {}

  ngOnInit() {}

  setLanguage(language: Language) {
    this.store.dispatch(LanguageActions.set({ language }));
  }
}
