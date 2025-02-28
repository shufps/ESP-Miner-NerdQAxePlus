import { NbToastrService } from '@nebular/theme';
import { NbAuthService, NbAuthResult, getDeepFromObject, NB_AUTH_OPTIONS, NB_AUTH_TOKEN_INTERCEPTOR_FILTER } from '@nebular/auth';
import { Router } from '@angular/router';
import {Observable} from 'rxjs/internal/Observable';
import {Inject, Injectable, Injector} from '@angular/core';
import {tap, catchError} from 'rxjs/operators';
import { HttpRequest, HttpInterceptor, HttpEvent, HttpHandler } from '@angular/common/http';
import { throwError } from 'rxjs';

@Injectable()
export class ResponseInterceptor implements HttpInterceptor {
    private strategy: string = 'email';
    constructor(
      private injector: Injector,
              @Inject(NB_AUTH_TOKEN_INTERCEPTOR_FILTER) protected filter,
              @Inject(NB_AUTH_OPTIONS) protected options = {},
              private router: Router,
              private nbToastService : NbToastrService){

            this.strategy = this.getConfigValue('forms.login.strategy');

    }
    intercept(request: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
      if (this.filter(request)) {
        return next.handle(request).pipe(catchError(err => {
            switch(err.status){
                case 401:
                  this.router.navigate(["/auth/login"]);
                  break;
            }
            //const error = err.error.message || err.statusText;
            return throwError(err);
        }));
      }else{
        return next.handle(request);
      }

    }

    private modifyBody(body: any) {
        /*
        * write your logic to modify the body
        * */
    }

    getConfigValue(key: string): any {
        return getDeepFromObject(this.options, key, null);
      }

      protected get authService(): NbAuthService {
        return this.injector.get(NbAuthService);
      }
}
