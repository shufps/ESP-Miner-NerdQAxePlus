import { TestBed } from '@angular/core/testing';
import { HTTP_INTERCEPTORS, HttpClient, provideHttpClient, withInterceptorsFromDi } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';

import { OtpAuthService } from './otp-auth.service';
import { OtpSessionInterceptor } from './otp-session.interceptor';

describe('OtpSessionInterceptor', () => {
  let http: HttpClient;
  let httpMock: HttpTestingController;
  const otpAuth = jasmine.createSpyObj<OtpAuthService>('OtpAuthService', [
    'getToken',
    'getExp',
    'clearSession',
  ]);

  beforeEach(() => {
    otpAuth.getToken.and.returnValue('session-token');
    otpAuth.getExp.and.returnValue(String(Date.now() + 60_000));

    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(withInterceptorsFromDi()),
        provideHttpClientTesting(),
        { provide: OtpAuthService, useValue: otpAuth },
        { provide: HTTP_INTERCEPTORS, useClass: OtpSessionInterceptor, multi: true },
      ],
    });

    http = TestBed.inject(HttpClient);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('attaches the OTP session header to relative API requests', () => {
    http.get('/api/system/info').subscribe();

    const req = httpMock.expectOne('/api/system/info');
    expect(req.request.headers.get('X-OTP-Session')).toBe('session-token');
    req.flush({});
  });

  it('does not leak the OTP session header to external origins', () => {
    http.get('https://api.github.com/repos/shufps/ESP-Miner-NerdQAxePlus/releases/latest').subscribe();

    const req = httpMock.expectOne('https://api.github.com/repos/shufps/ESP-Miner-NerdQAxePlus/releases/latest');
    expect(req.request.headers.has('X-OTP-Session')).toBeFalse();
    req.flush({});
  });
});
