import { Component } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { Store } from '@ngrx/store';
import * as fromI18n from './@i18n/reducers';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrl: './app.component.scss'
})
export class AppComponent {
  title = 'axe-os';

  constructor(
    private translate: TranslateService,
    private store: Store<fromI18n.State>
  ) {
    // Set available languages
    translate.addLangs(['en', 'fr', 'es', 'de', 'it', 'ro']);

    // Set default language
    translate.setDefaultLang('en');

    // Get language from localStorage or use browser language
    const savedLang = localStorage.getItem('language');
    const browserLang = navigator.language.split('-')[0];
    const defaultLang = savedLang || (translate.getLangs().includes(browserLang) ? browserLang : 'en');

    translate.use(defaultLang);

    // Listen to language changes from store
    this.store.select(fromI18n.selectLanguage).subscribe(language => {
      if (language) {
        translate.use(language);
        localStorage.setItem('language', language);
      }
    });
  }
}
