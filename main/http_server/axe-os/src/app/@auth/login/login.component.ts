import { Router } from '@angular/router';
import { Observable } from 'rxjs';
import { TranslateService } from '@ngx-translate/core';
import { Component, ChangeDetectorRef, Inject } from '@angular/core';
import {
  NbLoginComponent,
  NbAuthService,
  NB_AUTH_OPTIONS,
  NbAuthResult,
} from '@nebular/auth';
import { Store, select } from '@ngrx/store';
import * as fromI18n from '../../@i18n/reducers';
import { Language } from '../../@i18n/models/language.model';

@Component({
  selector: 'ngx-login',
  templateUrl: './login.component.html',
})
export class NgxLoginComponent extends NbLoginComponent {
  currentLanguage$: Observable<Language>;

  constructor(
    protected service: NbAuthService,
    @Inject(NB_AUTH_OPTIONS) protected options = {},
    protected cd: ChangeDetectorRef,
    protected router: Router,
    readonly store: Store<fromI18n.State>,
    readonly translate: TranslateService
  ) {
    super(service, options, cd, router);
  }

  login(): void {
    this.errors = [];
    this.messages = [];
    this.submitted = true;

    this.service
      .authenticate(this.strategy, this.user)
      .subscribe((result: NbAuthResult) => {
        this.submitted = false;

        if (result.isSuccess()) {
          this.messages = result.getMessages();
        } else {
          this.errors = result.getErrors();
        }
        let redirect =
          result.getResponse().body.data.redirect || result.getRedirect();

        if (redirect) {
          setTimeout(() => {
            return this.router.navigateByUrl(redirect);
          }, this.redirectDelay);
        }
        this.cd.detectChanges();
      });
  }
}
