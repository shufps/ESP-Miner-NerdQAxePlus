import { Injectable } from '@angular/core';
import { BehaviorSubject } from 'rxjs';
import { LocalStorageService } from './local-storage.service';

const STORAGE_KEY = 'experimentalDashboardEnabled';

@Injectable({
  providedIn: 'root',
})
export class ExperimentalDashboardService {
  private readonly _enabled$ = new BehaviorSubject<boolean>(this.localStorage.getBool(STORAGE_KEY));
  readonly enabled$ = this._enabled$.asObservable();

  constructor(private localStorage: LocalStorageService) {
    // In case other parts of the app update the flag, keep in sync.
    window.addEventListener('storage', (ev: StorageEvent) => {
      if (ev.key === STORAGE_KEY) {
        this._enabled$.next(this.localStorage.getBool(STORAGE_KEY));
      }
    });

    // Same-tab sync (we trigger this ourselves when toggling).
    window.addEventListener('experimentalDashboardChanged', (ev: Event) => {
      const detail = (ev as CustomEvent).detail;
      this._enabled$.next(!!detail);
    });
  }

  get enabled(): boolean {
    return this._enabled$.value;
  }

  setEnabled(enabled: boolean): void {
    const prev = this._enabled$.value;
    if (prev === enabled) return;

    // Wipe chart history once so the next dashboard state is clean.
    this.clearChartHistoryOnceSafe();

    this.localStorage.setBool(STORAGE_KEY, enabled);
    this._enabled$.next(enabled);
    window.dispatchEvent(new CustomEvent('experimentalDashboardChanged', { detail: enabled }));
  }

  private clearChartHistoryOnceSafe(): void {
    const anyWin = window as any;
    try {
      if (anyWin?.__nerdCharts?.clearChartHistoryInternal) {
        anyWin.__nerdCharts.clearChartHistoryInternal(true);
      } else {
        // The reliable wipe function is registered by HomeExperimentalComponent.
        // If it's not loaded yet, defer the wipe until it is.
        localStorage.setItem('__pendingChartHistoryWipe', '1');
      }
    } catch {
      // ignore
    }
  }
}
