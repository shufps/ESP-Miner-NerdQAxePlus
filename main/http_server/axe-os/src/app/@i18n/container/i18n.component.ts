import { OnInit, Directive } from '@angular/core';
import { select, Store } from '@ngrx/store';
import { TranslateService } from '@ngx-translate/core';
import { Observable } from 'rxjs';
import { Language } from '../models/language.model';
import * as fromI18n from '../reducers';

@Directive({

})
export class I18nComponent implements OnInit {
  currentLanguage$: Observable<Language>;
  translate: TranslateService;

  constructor(store: Store<fromI18n.State>, translate: TranslateService) {
    this.translate = translate;
    this.currentLanguage$ = store.pipe(select(fromI18n.getCurrentLanguage));
  

    translate.setDefaultLang('en');
    /*
    const browserLang = translate.getBrowserLang();

    let savedLang = localStorage.getItem("language");
    if(savedLang == undefined){
      savedLang = browserLang;
    }
    translate.use(browserLang.match(/en|fr|it|es|ro/) ? savedLang : 'en');
    */
    this.currentLanguage$.subscribe(
      (language) => {
        this.translate.use(language)
      });
  }

  ngOnInit(): void {

  }
}
