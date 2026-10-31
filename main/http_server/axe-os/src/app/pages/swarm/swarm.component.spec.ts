import { HttpClient, provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { FormBuilder } from '@angular/forms';
import { SwarmComponent } from './swarm.component';
import { SystemService } from '../../services/system.service';

describe('SwarmComponent discovery', () => {
  let component: SwarmComponent;
  let httpMock: HttpTestingController;
  let storage: jasmine.SpyObj<any>;
  const ip = '192.0.2.1';
  const validInfo = { ASICModel: 'FutureASIC', hashRate: 500, sharesAccepted: 10 };
  const validAsic = {
    ASICModel: 'FutureASIC', deviceModel: 'Future miner', asicCount: 4,
    frequencyOptions: [500], voltageOptions: [1200], swarmColor: 'green'
  };

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    httpMock = TestBed.inject(HttpTestingController);
    storage = jasmine.createSpyObj('LocalStorageService', ['getNumber', 'setNumber', 'setObject']);
    component = new SwarmComponent(
      new FormBuilder(), TestBed.inject(SystemService),
      jasmine.createSpyObj('NbToastrService', ['warning']),
      jasmine.createSpyObj('LoadingService', ['lockUIUntilComplete']),
      storage, TestBed.inject(HttpClient)
    );
    // Keep scans offline and limited to a single test address.
    spyOn<any>(component, 'calculateIpRange').and.returnValue({ start: 0xc0000201, end: 0xc0000201 });
    component.ipAddress = ip;
  });

  afterEach(() => {
    httpMock.verify();
    component.ngOnDestroy();
  });

  for (const path of ['automatic scan', 'manual addition']) {
    function discover(info: any, asic: any = null, asicMissing = false) {
      if (path === 'automatic scan') {
        component.scanNetwork();
      } else {
        component.form.patchValue({ manualAddIp: ip });
        component.add();
      }
      // Match the real SystemService request, including its history query parameters.
      httpMock.expectOne(req => req.url === `http://${ip}/api/system/info`).flush(info);
      const request = httpMock.expectOne(`http://${ip}/api/system/asic`);
      if (asicMissing) {
        request.flush('Not found', { status: 404, statusText: 'Not Found' });
      } else {
        request.flush(asic);
      }
    }

    describe(path, () => {
      const invalid: { name: string; info: any }[] = [
        { name: 'null', info: null },
        { name: 'an array', info: [] },
        { name: 'an unrelated error object', info: { error: 'unauthorized' } },
        { name: 'hashRate alone', info: { hashRate: 500 } },
        { name: 'ASICModel alone', info: { ASICModel: 'FutureASIC' } },
        { name: 'missing ASICModel', info: { hashRate: 500, sharesAccepted: 10 } },
      ];
      for (const value of [undefined, null, '', '   ', 1366, {}, []]) {
        invalid.push({ name: `invalid ASICModel ${JSON.stringify(value)}`, info: { ...validInfo, ASICModel: value } });
      }
      for (const field of ['hashRate', 'sharesAccepted']) {
        for (const value of [undefined, null, '0', '', false, {}, [], -1, NaN, Infinity, -Infinity]) {
          invalid.push({ name: `invalid ${field} ${String(value)}`, info: { ...validInfo, [field]: value } });
        }
      }
      for (const { name, info } of invalid) {
        it(`rejects ${name} even with valid ASIC enrichment`, () => {
          discover(info, validAsic);
          expect(component.swarm).toEqual([]);
          expect(storage.setObject.calls.allArgs().some(args => args[1].length > 0)).toBeFalse();
        });
      }

      it('accepts minimal legacy info without the optional ASIC endpoint', () => {
        discover({ ...validInfo, bestDiff: '1.5G' }, null, true);
        expect(component.swarm.length).toBe(1);
        expect(component.swarm[0]).toEqual(jasmine.objectContaining({
          ...validInfo, IP: ip, supportsAsicApi: false, bestDiff: 1_500_000_000
        }));
        expect(storage.setObject).toHaveBeenCalledWith('SWARM_DATA', component.swarm);
      });

      it('preserves zero rate and shares despite a nonzero ten-minute rate', () => {
        discover({ ...validInfo, hashRate: 0, hashRate_10m: 500, sharesAccepted: 0 }, {});
        expect(component.swarm.length).toBe(1);
        expect(component.swarm[0].hashRate).toBe(0);
        expect(component.swarm[0].sharesAccepted).toBe(0);
        expect(component.totals.hashRate).toBe(0);
        expect(component.swarm[0].supportsAsicApi).toBeFalse();
      });

      it('preserves valid optional ASIC enrichment', () => {
        discover(validInfo, validAsic);
        expect(component.swarm.length).toBe(1);
        expect(component.swarm[0]).toEqual(jasmine.objectContaining({
          ...validInfo, deviceModel: 'Future miner', asicCount: 4,
          swarmColor: 'green', supportsAsicApi: true
        }));
      });
    });
  }
});
