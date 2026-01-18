import {
  AfterViewInit,
  ChangeDetectionStrategy,
  ChangeDetectorRef,
  Component,
  NgZone,
  OnDestroy,
  ViewChild,
} from '@angular/core';
import {
  bufferTime,
  catchError,
  filter as rxFilter,
  interval,
  map,
  Observable,
  of,
  shareReplay,
  startWith,
  Subscription,
  switchMap,
  tap,
} from 'rxjs';
import { HttpErrorResponse } from '@angular/common/http';
import { CdkVirtualScrollViewport } from '@angular/cdk/scrolling';

import { NbThemeService, NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';

import { ISystemInfo } from '../../models/ISystemInfo';
import { LoadingService } from '../../services/loading.service';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { SystemService } from '../../services/system.service';
import { WebsocketService } from '../../services/web-socket.service';

type LogLine = { id: number; text: string; ts: number };

type LogsLockRecord = { ownerId: string; ts: number };
type LogsBroadcastMessage =
  | { type: 'LOCK_ACQUIRED'; ownerId: string }
  | { type: 'LOCK_RELEASED'; ownerId: string };

@Component({
  selector: 'app-logs',
  templateUrl: './system.component.html',
  styleUrls: ['./system.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class SystemComponent implements OnDestroy, AfterViewInit {
  /** Virtual scroll viewport used for high-performance log rendering. */
  @ViewChild('viewport') viewport?: CdkVirtualScrollViewport;

  /** Periodic system info stream. */
  public info$: Observable<ISystemInfo>;

  /** Full log buffer (capped by `maxLogs`). */
  public logs: LogLine[] = [];

  /** Filtered view of logs used by the virtual scroll viewport. */
  public filteredLogs: LogLine[] = [];

  /** Current log filter query. */
  public logFilterText = '';

  /** Whether the log viewer is visible and websocket is active. */
  public showLogs = false;

  /** Whether automatic scrolling is paused by user. */
  public stopScroll = false;

  /** Used to select light/dark logo variants. */
  public logoPrefix = '';

  /** Virtual scroll tuning (must match SCSS line-height). */
  public logItemSize = 18;
  public minBufferPx = 18 * 20;
  public maxBufferPx = 18 * 80;

  /** Max log count (fixed limit). */
  private maxLogs = 10000;

  /** Monotonic ID for stable `trackBy` in virtual scroll. */
  private nextLogId = 1;

  /** Subscription to websocket stream. */
  private websocketSubscription?: Subscription;

  /** True while a simple "scrollToIndex" RAF is pending (legacy helper). */
  private pendingScroll = false;

  /** "Sticky bottom" threshold in pixels. */
  private readonly SCROLL_STICKY_THRESHOLD_PX = 120;

  /** Active RAF id for smooth-follow animation loop. */
  private scrollRafId: number | null = null;

  /** Smooth-follow target scroll offset (px). */
  private scrollTargetOffset = 0;

  /** Ensures we queue at most one "follow update" RAF at a time. */
  private followRafScheduled = false;

  /** Storage key used for cross-tab master lock. */
  private readonly LOGS_LOCK_KEY = 'logs_ws_master';

  /** Unique tab identifier used as lock owner. */
  private readonly TAB_ID =
    typeof crypto !== 'undefined' && 'randomUUID' in crypto ? crypto.randomUUID() : `${Date.now()}-${Math.random()}`;

  /** BroadcastChannel for cross-tab lock coordination (if supported). */
  private bc?: BroadcastChannel;

  /** True if another tab currently holds the lock. */
  public logsLockedByOtherTab = false;

  /** Owner id of the tab holding the lock (debug/info). */
  public logsLockOwnerId: string | null = null;

  /** Heartbeat timer id to keep lock fresh while logs are running. */
  private lockHeartbeatTimer?: number;

  /** Event handler references (so we can remove them). */
  private onPageHide = () => {
    this.cleanupWebsocket();
    this.releaseLogsLock();
  };

  constructor(
    private websocketService: WebsocketService,
    private toastrService: NbToastrService,
    private themeService: NbThemeService,
    private systemService: SystemService,
    private loadingService: LoadingService,
    private translateService: TranslateService,
    private otpAuth: OtpAuthService,
    private ngZone: NgZone,
    private cdr: ChangeDetectorRef
  ) {
    this.logoPrefix = themeService.currentTheme === 'default' ? '' : '_dark';

    this.info$ = interval(5000).pipe(
      startWith(0),
      switchMap(() => this.systemService.getInfo()),
      map(info => {
        info.power = parseFloat(info.power.toFixed(1));
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));
        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));

        // Fixed log limit (independent of device heap)
        this.updateMaxLogsFromHeap(info.freeHeap);

        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.themeService.onThemeChange().subscribe((themeName) => {
      this.logoPrefix = themeName.name === 'default' ? '' : '_dark';
      this.cdr.markForCheck();
    });
  }

  /**
   * Initializes cross-tab coordination and ensures filtered view is aligned with
   * the current log buffer.
   */
  ngAfterViewInit(): void {
    this.applyFilter();
    this.setupBroadcastChannel();
    window.addEventListener('pagehide', this.onPageHide);
  }

  /**
   * Cleanup: close websocket, release cross-tab lock, stop timers and channels.
   */
  ngOnDestroy(): void {
    window.removeEventListener('pagehide', this.onPageHide);

    this.cleanupWebsocket();
    this.lockHeartbeatStop();
    this.releaseLogsLock();

    this.bc?.close();
  }

  /**
   * Unsubscribes from websocket stream and forces closing the underlying socket.
   * This is essential to free device-side sockets and avoid httpd accept errors.
   */
  private cleanupWebsocket(): void {
    this.websocketSubscription?.unsubscribe();
    this.websocketSubscription = undefined;
    this.websocketService.close();
  }

  /**
   * Toggles the visibility of logs and the websocket stream.
   *
   * Behavior:
   * - When turning ON: tries to acquire cross-tab lock. If another tab owns it,
   *   shows an info message and does not connect.
   * - When turning OFF: closes websocket, clears logs (free memory), releases lock.
   *
   * Input: none
   * Output: void
   */
  public toggleLogs() {
    // If we are about to turn logs ON, ensure single-tab ownership
    if (!this.showLogs) {
      if (!this.tryAcquireLogsLock()) {
        // Show a friendly message (translation key optional)
        const msg =
          this.translateService.instant('SYSTEM.LOGS_ALREADY_OPEN_OTHER_TAB') ||
          'Logs are already open in another browser tab.';
        const title = this.translateService.instant('COMMON.INFO') || 'Info';
        this.toastrService.warning(msg, title);
        return;
      }
    }

    this.showLogs = !this.showLogs;

    if (this.showLogs) {
      // Ensure initial render
      this.applyFilter();

      // Keep lock alive while websocket is running
      this.lockHeartbeatStart();

      // Process websocket outside Angular; only re-enter once per batch.
      this.ngZone.runOutsideAngular(() => {
        this.websocketSubscription = this.websocketService
          .connect()
          .pipe(
            bufferTime(50),
            rxFilter((batch) => batch.length > 0)
          )
          .subscribe({
            next: (batch) => {
              const lines = batch.map((v) => String(v));
              this.ngZone.run(() => this.appendLogs(lines));
            },
          });
      });

      this.cdr.markForCheck();
    } else {
      this.cleanupWebsocket();
      this.clearLogs();

      this.lockHeartbeatStop();
      this.releaseLogsLock();

      this.cdr.markForCheck();
    }
  }

  /**
   * Sets the maximum number of stored log lines.
   * The limit is fixed to 10,000 lines (independent of device free heap).
   *
   * Input:
   *  @param _freeHeap number - Free heap memory reported by the device (bytes)
   *
   * Output: void
   * Side effects: updates `maxLogs`
   */
  private updateMaxLogsFromHeap(_freeHeap: number) {
    this.maxLogs = 10000;
  }

  /**
   * Clears all log-related state and releases memory.
   *
   * Responsibilities:
   *  - Cancels any active smooth-scroll animation
   *  - Empties log buffers (`logs` and `filteredLogs`)
   *  - Resets internal log ID counter
   *  - Resets viewport scroll position
   *  - Resets autoscroll pause state
   *  - Triggers OnPush change detection
   *
   * Input: none
   * Output: void
   */
  private clearLogs() {
    this.stopAutoScrollAnimation();

    this.logs = [];
    this.filteredLogs = [];
    this.nextLogId = 1;

    this.viewport?.scrollToOffset(0);

    this.stopScroll = false;

    this.cdr.markForCheck();
  }

  /**
   * Applies the current `logFilterText` to the main log buffer and updates
   * `filteredLogs` which backs the virtual scroll viewport.
   *
   * Input: none
   * Output: void
   */
  public applyFilter() {
    const q = (this.logFilterText || '').toLowerCase().trim();
    this.filteredLogs = !q ? this.logs.slice() : this.logs.filter((l) => l.text.toLowerCase().includes(q));
    this.cdr.markForCheck();
  }

  /** TrackBy function for virtual scroll to reduce DOM churn. */
  public trackByLogId = (_: number, log: LogLine) => log.id;

  /**
   * Appends a batch of new log lines to the internal log buffer.
   *
   * Responsibilities:
   *  - Converts raw string lines into LogLine objects with incremental IDs
   *  - Appends them to the main log buffer
   *  - Enforces the `maxLogs` limit (drops oldest entries)
   *  - Re-applies the active text filter
   *  - Triggers OnPush change detection
   *  - Schedules a frame-synchronized smooth auto-scroll update
   *
   * Input:
   *  @param lines string[] - Raw log lines received from the websocket (already batched)
   *
   * Output: void
   */
  private appendLogs(lines: string[]) {
    const newLines: LogLine[] = lines.map((text) => ({
      id: this.nextLogId++,
      text,
      ts: Date.now(),
    }));

    this.logs.push(...newLines);

    if (this.logs.length > this.maxLogs) {
      this.logs.splice(0, this.logs.length - this.maxLogs);
    }

    const q = (this.logFilterText || '').toLowerCase().trim();
    this.filteredLogs = !q ? this.logs.slice() : this.logs.filter((l) => l.text.toLowerCase().includes(q));

    this.cdr.markForCheck();

    if (this.showLogs && !this.stopScroll) {
      this.scheduleSmoothFollowNextFrame();
    }
  }

  /**
   * Schedules a single frame-synchronized smooth-follow update.
   * Ensures that only one RAF callback is queued at a time.
   *
   * Input: none
   * Output: void
   */
  private scheduleSmoothFollowNextFrame() {
    if (this.followRafScheduled) return;
    if (this.stopScroll) return;

    this.followRafScheduled = true;

    requestAnimationFrame(() => {
      this.followRafScheduled = false;
      this.scheduleSmoothFollowIfSticky();
    });
  }

  /**
   * Toggles automatic scrolling of the log viewport.
   *
   * STOP -> START:
   *  - jumps immediately to the latest log line (bottom),
   *  - then resumes smooth follow for incoming logs.
   *
   * START -> STOP:
   *  - cancels any pending requestAnimationFrame based smooth scrolling.
   *
   * Input: none
   * Output: void
   */
  public toggleScrolling() {
    this.stopScroll = !this.stopScroll;

    if (this.stopScroll) {
      this.stopAutoScrollAnimation();
    } else {
      this.jumpToBottomNow();
      this.scheduleSmoothFollowIfSticky();
    }

    this.cdr.markForCheck();
  }

  /**
   * Cancels active smooth-follow animation driven by requestAnimationFrame.
   *
   * Input: none
   * Output: void
   */
  private stopAutoScrollAnimation() {
    if (this.scrollRafId !== null) {
      cancelAnimationFrame(this.scrollRafId);
      this.scrollRafId = null;
    }
    this.followRafScheduled = false;
  }

  /**
   * Immediately scrolls the virtual viewport to the last visible log line (bottom),
   * without animation. Also synchronizes the internal smooth-follow target.
   *
   * Input: none
   * Output: void
   */
  private jumpToBottomNow() {
    const vp = this.viewport;
    if (!vp) return;

    const len = this.filteredLogs.length;
    if (len <= 0) return;

    // Ensures viewport measurements/range are up-to-date (useful after show/hide/layout changes).
    vp.checkViewportSize();

    const target = Math.max(0, (len - 1) * this.logItemSize);
    vp.scrollToOffset(target);

    this.scrollTargetOffset = target;
    this.stopAutoScrollAnimation();
  }

  /**
   * Returns true if the user is at (or near) the bottom of the log output.
   * Prevents autoscroll when the user manually scrolls up.
   *
   * Input: none
   * Output: boolean
   */
  private isNearBottom(): boolean {
    const vp = this.viewport;
    if (!vp) return true;

    const dist = vp.measureScrollOffset('bottom');
    return dist <= this.SCROLL_STICKY_THRESHOLD_PX;
  }

  /**
   * Schedules/resumes smooth-follow scrolling when:
   *  - autoscroll is enabled, and
   *  - the user is near the bottom.
   *
   * Input: none
   * Output: void
   */
  private scheduleSmoothFollowIfSticky() {
    if (this.stopScroll) return;
    if (!this.isNearBottom()) return;

    const len = this.filteredLogs.length;
    if (len <= 0) return;

    this.scrollTargetOffset = Math.max(0, (len - 1) * this.logItemSize);

    if (this.scrollRafId === null) {
      this.runSmoothFollow();
    }
  }

  /**
   * Smooth-follow animation loop.
   * Each frame moves the scroll position a fraction towards the latest target offset,
   * creating a soft "overlap" feel.
   *
   * Input: none
   * Output: void
   */
  private runSmoothFollow() {
    if (this.stopScroll) {
      this.stopAutoScrollAnimation();
      return;
    }

    const vp = this.viewport;
    if (!vp) {
      this.scrollRafId = null;
      return;
    }

    const current = vp.measureScrollOffset('top');
    const target = this.scrollTargetOffset;
    const diff = target - current;

    if (Math.abs(diff) < 2) {
      vp.scrollToOffset(target);
      this.scrollRafId = null;
      return;
    }

    // Easing factor: smaller = smoother, larger = tighter follow.
    const step = diff * 0.22;
    vp.scrollToOffset(current + step);

    this.scrollRafId = requestAnimationFrame(() => this.runSmoothFollow());
  }

  /**
   * Called after filter changes to optionally keep the viewport following the bottom.
   *
   * Input: none
   * Output: void
   */
  public onFilterMaybeScroll() {
    if (this.showLogs && !this.stopScroll) {
      this.scheduleSmoothFollowIfSticky();
    }
  }

  /**
   * (Optional legacy helper) Scrolls to the bottom using `scrollToIndex`.
   * You generally do NOT need this when smooth follow is enabled.
   */
  private scheduleScrollToBottomIfSticky() {
    if (!this.isNearBottom()) return;
    if (this.pendingScroll) return;

    this.pendingScroll = true;
    requestAnimationFrame(() => {
      this.pendingScroll = false;

      const vp = this.viewport;
      if (!vp) return;

      const len = this.filteredLogs.length;
      if (len > 0) {
        vp.scrollToIndex(len - 1, 'auto');
      }
    });
  }

  /**
   * Initiates device restart with OTP flow, and displays a toast notification.
   *
   * Input: none
   * Output: void
   */
  public restart() {
    this.otpAuth
      .ensureOtp$(
        '',
        this.translateService.instant('SECURITY.OTP_TITLE'),
        this.translateService.instant('SECURITY.OTP_HINT'),
        { disableOtp: true }
      )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.restart('', totp).pipe(
            tap(() => {}),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
          this.toastrService.danger(
            this.translateService.instant('SYSTEM.RESTART_FAILED'),
            this.translateService.instant('COMMON.ERROR')
          );
          return of(null);
        })
      )
      .subscribe((res) => {
        if (res !== null) {
          this.toastrService.success(
            this.translateService.instant('SYSTEM.RESTART_SUCCESS'),
            this.translateService.instant('COMMON.SUCCESS')
          );
        }
      });
  }

  /**
   * Initiates device shutdown with OTP flow, and displays a toast notification.
   *
   * Input: none
   * Output: void
   */
  public shutdown() {
    this.otpAuth
      .ensureOtp$(
        '',
        this.translateService.instant('SECURITY.OTP_TITLE'),
        this.translateService.instant('SECURITY.OTP_HINT'),
        { disableOtp: true }
      )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.shutdown('', totp).pipe(
            tap(() => {}),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
          this.toastrService.danger(
            this.translateService.instant('SYSTEM.SHUTDOWN_FAILED'),
            this.translateService.instant('COMMON.ERROR')
          );
          return of(null);
        })
      )
      .subscribe((res) => {
        if (res !== null) {
          this.toastrService.success(
            this.translateService.instant('SYSTEM.SHUTDOWN_SUCCESS'),
            this.translateService.instant('COMMON.SUCCESS')
          );
        }
      });
  }

  /**
   * Returns a translated tooltip string based on RSSI levels.
   *
   * Input:
   *  @param rssi number - WiFi RSSI in dBm
   * Output:
   *  string - translated tooltip label
   */
  public getRssiTooltip(rssi: number): string {
    if (rssi <= -85) return this.translateService.instant('SYSTEM.SIGNAL_VERY_WEAK');
    if (rssi <= -75) return this.translateService.instant('SYSTEM.SIGNAL_WEAK');
    if (rssi <= -65) return this.translateService.instant('SYSTEM.SIGNAL_MODERATE');
    if (rssi <= -55) return this.translateService.instant('SYSTEM.SIGNAL_STRONG');
    return this.translateService.instant('SYSTEM.SIGNAL_EXCELLENT');
  }

  /**
   * Computes boot time from uptime seconds.
   *
   * Input:
   *  @param uptimeSeconds number - device uptime in seconds
   * Output:
   *  number - timestamp (ms since epoch)
   */
  public getBootTime(uptimeSeconds: number): number {
    return Date.now() - uptimeSeconds * 1000;
  }

  /**
   * Opens an external URL in a new tab with safe flags.
   *
   * Input:
   *  @param url string - destination
   * Output: void
   */
  openLink(url: string): void {
    window.open(url, '_blank', 'noopener,noreferrer');
  }

  /**
   * Sets up BroadcastChannel listeners used to coordinate which tab is allowed
   * to own the websocket connection.
   *
   * Input: none
   * Output: void
   */
  private setupBroadcastChannel() {
    if (!('BroadcastChannel' in window)) return;

    this.bc = new BroadcastChannel('logs_ws_channel');
    this.bc.onmessage = (ev) => {
      const msg = ev.data as LogsBroadcastMessage;
      if (!msg?.type) return;

      if (msg.type === 'LOCK_ACQUIRED' && msg.ownerId !== this.TAB_ID) {
        this.logsLockedByOtherTab = true;
        this.logsLockOwnerId = msg.ownerId;
        this.cdr.markForCheck();
      }

      if (msg.type === 'LOCK_RELEASED') {
        this.logsLockedByOtherTab = false;
        this.logsLockOwnerId = null;
        this.cdr.markForCheck();
      }
    };
  }

  /**
   * Tries to acquire the "logs master" lock. Only the lock owner may open the websocket.
   * Uses a stale timeout so the lock can be recovered if a tab crashes.
   *
   * Input: none
   * Output: boolean - true if this tab acquired the lock and may start logs.
   */
  private tryAcquireLogsLock(): boolean {
    const now = Date.now();
    const raw = localStorage.getItem(this.LOGS_LOCK_KEY);

    if (raw) {
      try {
        const lock = JSON.parse(raw) as LogsLockRecord;

        // If lock is fresh and owned by another tab, deny
        const isFresh = (now - (lock.ts ?? 0)) < 15_000;
        if (lock?.ownerId && lock.ownerId !== this.TAB_ID && isFresh) {
          this.logsLockedByOtherTab = true;
          this.logsLockOwnerId = lock.ownerId;
          return false;
        }
      } catch {
        // ignore and overwrite below
      }
    }

    // Acquire lock for this tab
    localStorage.setItem(this.LOGS_LOCK_KEY, JSON.stringify({ ownerId: this.TAB_ID, ts: now } as LogsLockRecord));
    this.logsLockedByOtherTab = false;
    this.logsLockOwnerId = this.TAB_ID;

    // Notify other tabs
    this.bc?.postMessage({ type: 'LOCK_ACQUIRED', ownerId: this.TAB_ID } satisfies LogsBroadcastMessage);

    return true;
  }

  /**
   * Refreshes the lock heartbeat timestamp so other tabs know this owner is still alive.
   *
   * Input: none
   * Output: void
   */
  private refreshLogsLockHeartbeat() {
    const raw = localStorage.getItem(this.LOGS_LOCK_KEY);
    if (!raw) return;

    try {
      const lock = JSON.parse(raw) as LogsLockRecord;
      if (lock.ownerId === this.TAB_ID) {
        localStorage.setItem(this.LOGS_LOCK_KEY, JSON.stringify({ ownerId: this.TAB_ID, ts: Date.now() } as LogsLockRecord));
      }
    } catch {
      // ignore
    }
  }

  /**
   * Releases the lock if (and only if) it is owned by this tab.
   *
   * Input: none
   * Output: void
   */
  private releaseLogsLock() {
    const raw = localStorage.getItem(this.LOGS_LOCK_KEY);
    if (!raw) {
      this.logsLockedByOtherTab = false;
      this.logsLockOwnerId = null;
      return;
    }

    try {
      const lock = JSON.parse(raw) as LogsLockRecord;
      if (lock.ownerId === this.TAB_ID) {
        localStorage.removeItem(this.LOGS_LOCK_KEY);
        this.bc?.postMessage({ type: 'LOCK_RELEASED', ownerId: this.TAB_ID } satisfies LogsBroadcastMessage);
      }
    } catch {
      localStorage.removeItem(this.LOGS_LOCK_KEY);
    }

    this.logsLockedByOtherTab = false;
    this.logsLockOwnerId = null;
  }

  /**
   * Starts periodic heartbeat updates while logs are active, preventing other tabs
   * from treating this lock as stale.
   *
   * Input: none
   * Output: void
   */
  private lockHeartbeatStart() {
    this.lockHeartbeatStop();
    this.lockHeartbeatTimer = window.setInterval(() => this.refreshLogsLockHeartbeat(), 5000);
  }

  /**
   * Stops periodic lock heartbeat updates.
   *
   * Input: none
   * Output: void
   */
  private lockHeartbeatStop() {
    if (this.lockHeartbeatTimer) {
      clearInterval(this.lockHeartbeatTimer);
      this.lockHeartbeatTimer = undefined;
    }
  }


  public formatLogForUi(text: string): string {
    // Replace ESC[0;32mI -> ESC[0;32m₿ (keeps original ANSI styling)
    return (text ?? '').replace(/\x1b\[0;32mI/g, '\x1b[0;32m₿');
  }

  public downloadLogs(): void {
    const content = this.buildDownloadTextFromLogs();
    const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });

    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `logs_${this.formatFilenameStamp(new Date())}.txt`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    window.URL.revokeObjectURL(url);
  }

  private buildDownloadTextFromLogs(): string {
    const out: string[] = [];

    for (const l of this.logs) {
      const time = this.formatHms(l.ts);

      const rawLines = String(l.text ?? '').split(/\r?\n/);
      for (const raw of rawLines) {
        const cleaned = this.cleanLogLineForExport(raw);
        if (!cleaned) continue;
        out.push(`${time} ${cleaned}`);
      }
    }

    return out.join('\n') + (out.length ? '\n' : '');
  }

  private cleanLogLineForExport(line: string): string {
    let t = String(line ?? '');

    // Replace the colored "I" marker directly (if present)
    t = t.replace(/\x1b\[0;32mI/g, '₿');

    // Strip ANSI sequences for a clean text file
    t = this.stripAnsi(t);

    // --- Anonymize sensitive values for exported logs

    // stratum_api: replace params[0] with USER.WORKER
    if (t.includes('stratum_api:') && t.includes('"params"')) {
      t = t.replace(/(\"params\"\s*:\s*\[\s*\")(?:[^\"]*)(\")/, '$1USER.WORKER$2');
    }

    // http_cors: hide client IP
    t = t.replace(/(http_cors:\s*Client IP:\s*)([0-9]{1,3}(?:\.[0-9]{1,3}){3})/g, '$1127.0.0.1');

    // ping task (pri): hide bytes-from IP
    t = t.replace(/(ping task \(pri\):\s*\d+\s+bytes from\s*)([0-9]{1,3}(?:\.[0-9]{1,3}){3})/g, '$1127.0.0.1');

    // ping task (pri): hide PING host + IP
    t = t.replace(/(ping task \(pri\):\s*PING\s*)([^ ]+)(\s*\()([0-9]{1,3}(?:\.[0-9]{1,3}){3})(\)\s*:)/g, '$1locahost.host$3127.0.0.1$5');

    // ping task (pri): hide ping statistics host
    t = t.replace(
      /(ping task \(pri\):\s*---\s*)(.+?)(\s+ping statistics\s*---)/g,
      '$1Nerd*Pool$3'
    );

    // InfluxDB: normalize URL (host/port) and query params
    if (/InfluxDB:\s*URL:/i.test(t)) {
      t = t.replace(
        /(InfluxDB:\s*URL:\s*)https?:\/\/[^\/\s]+(\/\S*)?/i,
        (_m, p1, p2) => `${p1}http://localhost.local:1234${p2 ?? '/'}`
      );
      t = t.replace(/([?&]bucket=)[^&\s]*/i, '$1xBucket');
      t = t.replace(/([?&]org=)[^&\s]*/i, '$1xOrg');
    }

    // InfluxDB: anonymize POST payload
    if (/InfluxDB:\s*POST:/i.test(t)) {
      t = t.replace(/(InfluxDB:\s*POST:\s*).*/i, '$1Anonymize sensitive values.');
    }

    // If it already starts with a plain "I ", convert that too
    t = t.replace(/^I(\s)/, '₿$1');

    // Drop empty lines
    t = t.replace(/\r/g, '').trimEnd();
    return t.trim().length ? t : '';
  }

  private stripAnsi(text: string): string {
    return String(text ?? '').replace(/\x1b\[[0-9;]*m/g, '');
  }

  private formatHms(ts: number): string {
    const d = new Date(ts);
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    const ss = String(d.getSeconds()).padStart(2, '0');
    return `${hh}:${mm}:${ss}`;
  }

  private formatFilenameStamp(d: Date): string {
    const yyyy = d.getFullYear();
    const mm = String(d.getMonth() + 1).padStart(2, '0');
    const dd = String(d.getDate()).padStart(2, '0');
    const hh = String(d.getHours()).padStart(2, '0');
    const mi = String(d.getMinutes()).padStart(2, '0');
    const ss = String(d.getSeconds()).padStart(2, '0');
    return `${yyyy}${mm}${dd}_${hh}${mi}${ss}`;
  }

}
