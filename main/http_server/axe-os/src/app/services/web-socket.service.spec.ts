import { TestBed } from '@angular/core/testing';

import { WebsocketService } from './web-socket.service';

describe('WebsocketService', () => {
  let service: WebsocketService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(WebsocketService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('uses wss when the UI is served over https', () => {
    expect((service as any).getWebSocketUrl({ protocol: 'https:', host: 'miner.example' }))
      .toBe('wss://miner.example/api/ws');
  });

  it('keeps ws when the UI is served over http', () => {
    expect((service as any).getWebSocketUrl({ protocol: 'http:', host: '192.168.1.42' }))
      .toBe('ws://192.168.1.42/api/ws');
  });
});
