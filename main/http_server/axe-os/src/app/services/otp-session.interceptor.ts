import { Injectable } from '@angular/core';
import {
  HttpInterceptor, HttpRequest, HttpHandler, HttpEvent, HttpErrorResponse,
} from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import { catchError } from 'rxjs/operators';
import { OtpAuthService } from '../services/otp-auth.service';



@Injectable()
export class OtpSessionInterceptor implements HttpInterceptor {
  constructor(private otpAuth: OtpAuthService) { }

  intercept(req: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
    let authReq = req;

    try {
      // Read token once per request
      const token = this.otpAuth.getToken();
      const expRaw = this.otpAuth.getExp();
      const exp = Number.parseInt(expRaw, 10) || 0;

      // Only attach header if token exists and is not expired
      if (token && Date.now() < exp) {
        authReq = req.clone({ setHeaders: { 'X-OTP-Session': token } });
      }
    } catch {
      // Ignore storage errors (Safari private mode, etc.)
    }

    return next.handle(authReq).pipe(
      catchError((err: any) => {
        // If the server rejects the request due to auth, clear the local token
        if (err instanceof HttpErrorResponse && err.status === 401) {
          try {
            this.otpAuth.clearSession();
          } catch { /* ignore */ }
        }
        return throwError(() => err);
      }),
    );
  }
}
