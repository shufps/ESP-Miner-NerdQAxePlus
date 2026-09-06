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

  it('downloads a core dump as a blob with a one-shot OTP', () => {
    let elfSha256 = '';
    service.downloadCoreDump('123456').subscribe((response) => {
      elfSha256 = response.headers.get('X-ESP-App-ELF-SHA256') ?? '';
    });

    const req = httpMock.expectOne('/api/system/coredump');
    expect(req.request.method).toBe('GET');
    expect(req.request.responseType).toBe('blob');
    expect(req.request.headers.get('X-TOTP')).toBe('123456');
    req.flush(new Blob(), { headers: { 'X-ESP-App-ELF-SHA256': '0123456789abcdef' } });
    expect(elfSha256).toBe('0123456789abcdef');
  });
});
