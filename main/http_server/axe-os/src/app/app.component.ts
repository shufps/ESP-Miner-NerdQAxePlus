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
    translate.addLangs(['en', 'fr', 'es', 'de', 'it', 'ro', 'pl']);

    translate.setDefaultLang('en');

    // Store holds the active language (initialized from localStorage or 'en').
    // Subscribing here covers both initial load and user-triggered changes.
    this.store.select(fromI18n.selectLanguage).subscribe(language => {
      translate.use(language);
      localStorage.setItem('language', language);
    });
  }
}
