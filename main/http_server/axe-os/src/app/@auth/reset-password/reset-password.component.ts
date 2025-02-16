import { Observable } from 'rxjs';
import { TranslateService } from '@ngx-translate/core';
import { NbResetPasswordComponent, NB_AUTH_OPTIONS, NbAuthService, NbAuthJWTToken } from '@nebular/auth';
import { ChangeDetectionStrategy, ChangeDetectorRef, Component, Inject } from '@angular/core';
import { Router } from '@angular/router';
import { Store, select } from '@ngrx/store';
import { Language } from '../../@i18n/models/language.model';


@Component({
  selector: 'ngx-reset-password-page',
  templateUrl: './reset-password.component.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class NgxResetPasswordComponent extends NbResetPasswordComponent {
    currentLanguage$: Observable<Language>;


    constructor(protected authService: NbAuthService,
        @Inject(NB_AUTH_OPTIONS) protected options = {},
        protected cd: ChangeDetectorRef,
        protected router: Router) {
    
        super(authService,options,cd,router);
      }

}