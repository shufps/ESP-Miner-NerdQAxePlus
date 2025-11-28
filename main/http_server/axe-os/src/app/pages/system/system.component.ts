import { AfterViewChecked, Component, ElementRef, OnDestroy, ViewChild } from '@angular/core';
import { interval, map, tap, catchError, of, Observable, shareReplay, startWith, Subscription, switchMap } from 'rxjs';
import { SystemService } from '../../services/system.service';
import { WebsocketService } from '../../services/web-socket.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { NbToastrService, NbThemeService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { LoadingService } from '../../services/loading.service';

@Component({
  selector: 'app-logs',
  templateUrl: './system.component.html',
  styleUrl: './system.component.scss'
})
export class SystemComponent implements OnDestroy, AfterViewChecked {
  @ViewChild('scrollContainer') private scrollContainer!: ElementRef;
  public info$: Observable<ISystemInfo>;

  public logs: string[] = [];
  public logFilterText: string = '';
  public showLogs = false;
  public stopScroll = false;
  public logoPrefix: string = '';

  private websocketSubscription?: Subscription;

  constructor(
    private websocketService: WebsocketService,
    private toastrService: NbToastrService,
    private themeService: NbThemeService,
    private systemService: SystemService,
    private loadingService: LoadingService,
    private translateService: TranslateService,
    private otpAuth: OtpAuthService,
  ) {
    this.logoPrefix = themeService.currentTheme === 'default' ? '' : '_dark';

    this.info$ = interval(5000).pipe(
      startWith(0),
      switchMap(() => this.systemService.getInfo(0)),
      map(info => {
        info.power = parseFloat(info.power.toFixed(1));
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));
        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));
        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.themeService.onThemeChange()
      .subscribe(themeName => {
        this.logoPrefix = themeName.name === 'default' ? '' : '_dark';
      });
  }

  ngOnDestroy(): void {
    this.cleanupWebsocket();
  }

  private cleanupWebsocket(): void {
    // Unsubscribe from messages
    this.websocketSubscription?.unsubscribe();
    this.websocketSubscription = undefined;

    // And force-close underlying socket
    this.websocketService.close();
  }

  public toggleLogs() {
    this.showLogs = !this.showLogs;

    if (this.showLogs) {
      this.websocketSubscription = this.websocketService.connect().subscribe({
        next: (val) => {
          const valStr = String(val);
          if (!this.logFilterText || valStr.toLowerCase().includes(this.logFilterText.toLowerCase())) {
            this.logs.push(valStr);
            if (this.logs.length > 256) {
              this.logs.shift();
            }
          }
        }
      });
    } else {
      this.cleanupWebsocket();
    }
  }

  ngAfterViewChecked(): void {
    if (this.stopScroll == true) {
      return;
    }
    if (this.scrollContainer?.nativeElement != null) {
      this.scrollContainer.nativeElement.scrollTo({ left: 0, top: this.scrollContainer.nativeElement.scrollHeight, behavior: 'smooth' });
    }
  }

  public restart() {
    this.otpAuth.ensureOtp$(
      "",
      this.translateService.instant('SECURITY.OTP_TITLE'),
      this.translateService.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.restart("", totp).pipe(
            // drop session on reboot
            tap(() => { }),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
          this.toastrService.danger(this.translateService.instant('SYSTEM.RESTART_FAILED'), this.translateService.instant('COMMON.ERROR'));
          return of(null);
        })
      )
      .subscribe(res => {
        if (res !== null) {
          this.toastrService.success(this.translateService.instant('SYSTEM.RESTART_SUCCESS'), this.translateService.instant('COMMON.SUCCESS'));
        }
      });
  }

  public getRssiTooltip(rssi: number): string {
    if (rssi <= -85) return this.translateService.instant('SYSTEM.SIGNAL_VERY_WEAK');
    if (rssi <= -75) return this.translateService.instant('SYSTEM.SIGNAL_WEAK');
    if (rssi <= -65) return this.translateService.instant('SYSTEM.SIGNAL_MODERATE');
    if (rssi <= -55) return this.translateService.instant('SYSTEM.SIGNAL_STRONG');
    return this.translateService.instant('SYSTEM.SIGNAL_EXCELLENT');
  }



  openLink(url: string): void {
    // Open external link in a new tab
    window.open(url, '_blank', 'noopener,noreferrer');
  }
}
