import { Component, OnInit } from '@angular/core';
import { Store } from '@ngrx/store';
import { Observable } from 'rxjs';
import * as fromI18n from '../../reducers';
import { LanguageActions } from '../../actions';
import { Language } from '../../models/language.model';

@Component({
  selector: 'app-language-selector',
  templateUrl: './language-selector.component.html',
  styleUrls: ['./language-selector.component.scss']
})
export class LanguageSelectorComponent implements OnInit {
  currentLanguage$: Observable<Language>;

  constructor(private readonly store: Store<fromI18n.State>) {
    this.currentLanguage$ = this.store.select(fromI18n.selectLanguage);
  }

  ngOnInit() {}

  setLanguage(language: Language) {
    this.store.dispatch(LanguageActions.set({ language }));
  }
}
