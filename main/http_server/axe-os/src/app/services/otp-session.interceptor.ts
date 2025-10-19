import { Injectable } from '@angular/core';
import { HttpInterceptor, HttpRequest, HttpHandler, HttpEvent } from '@angular/common/http';
import { Observable } from 'rxjs';

@Injectable()
export class OtpSessionInterceptor implements HttpInterceptor {
  intercept(req: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {
    try {
      const token = localStorage.getItem('otpSessionToken');
      const exp   = parseInt(localStorage.getItem('otpSessionExpiry') || '0', 10);
      const valid = token && Date.now() < exp;
      if (valid) {
        const cloned = req.clone({ setHeaders: { 'X-OTP-Session': token! } });
        return next.handle(cloned);
      }
    } catch {}
    return next.handle(req);
  }
}
