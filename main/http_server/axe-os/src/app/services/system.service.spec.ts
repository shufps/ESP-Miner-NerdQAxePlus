import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideHttpClientTesting, HttpTestingController } from '@angular/common/http/testing';

import { SystemService } from './system.service';

describe('SystemService', () => {
  let service: SystemService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
      ],
    });
    service = TestBed.inject(SystemService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('sendAlertTest sends one-shot OTP using the backend accepted X-TOTP header', () => {
    service.sendAlertTest('', '123456').subscribe();

    const req = httpMock.expectOne('/api/v2/alert/test');
    expect(req.request.method).toBe('POST');
    expect(req.request.headers.get('X-TOTP')).toBe('123456');
    expect(req.request.headers.has('X-OTP-Code')).toBeFalse();
    req.flush('ok');
  });
});
