import { Injectable, OnDestroy } from '@angular/core';
import { webSocket, WebSocketSubject } from 'rxjs/webSocket';

@Injectable({
  providedIn: 'root'
})
export class WebsocketService implements OnDestroy {

  private socket$?: WebSocketSubject<string>;

  public connect(): WebSocketSubject<string> {
    // Create socket if not present or already closed
    if (!this.socket$ || this.socket$.closed) {
      this.socket$ = webSocket<string>({
        url: `ws://${window.location.host}/api/ws`,
        deserializer: (e: MessageEvent) => e.data
      });
    }
    return this.socket$;
  }

  public close(): void {
    if (!this.socket$) return;

    try {
      // 1) Try to close gracefully (sends a Close frame)
      if (!this.socket$.closed) {
        this.socket$.complete();
      }
      // 2) Ensure the underlying native WebSocket is closed as well.
      // RxJS WebSocketSubject wraps the native socket; in some shutdown scenarios
      // (tab close / reload) we want to be extra sure the connection is terminated.
      const ws: WebSocket | undefined = (this.socket$ as any)?._socket;
      if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
        ws.close(1000, 'client closing');
      }
    } catch {
      // Ignore close errors — we just want to prevent leaking sockets
    } finally {
      this.socket$ = undefined;
    }
  }

  ngOnDestroy(): void {
    this.close();
  }
}
