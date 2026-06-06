import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';

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

  it('sends X-TOTP when resetting stats with a one-shot code', () => {
    service.resetStats('', '123456').subscribe();

    const req = httpMock.expectOne('/api/system/reset-stats');
    expect(req.request.method).toBe('POST');
    expect(req.request.headers.get('X-TOTP')).toBe('123456');
    req.flush('', { status: 204, statusText: 'No Content' });
  });
});
