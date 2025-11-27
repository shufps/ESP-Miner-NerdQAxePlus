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
    // Ensure underlying websocket is really closed
    if (this.socket$ && !this.socket$.closed) {
      // complete() schickt Close-Frame und schließt den Socket
      this.socket$.complete();
    }
    this.socket$ = undefined;
  }

  ngOnDestroy(): void {
    this.close();
  }
}
