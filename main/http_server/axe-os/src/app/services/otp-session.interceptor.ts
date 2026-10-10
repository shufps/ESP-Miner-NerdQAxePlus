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

  private shouldAttachOtpSession(url: string): boolean {
    try {
      const requestUrl = new URL(url, window.location.origin);
      return requestUrl.origin === window.location.origin;
    } catch {
      return false;
    }
  }

  intercept(req: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
    let authReq = req;

    try {
      // Read token once per request
      const token = this.otpAuth.getToken();
      const expRaw = this.otpAuth.getExp();
      const exp = Number.parseInt(expRaw, 10) || 0;

      // Only attach header if token exists, is not expired, and the request targets this UI origin.
      if (token && Date.now() < exp && this.shouldAttachOtpSession(req.url)) {
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
