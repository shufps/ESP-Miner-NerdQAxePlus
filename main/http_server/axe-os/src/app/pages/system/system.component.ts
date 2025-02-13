import { AfterViewChecked, Component, ElementRef, OnDestroy, ViewChild } from '@angular/core';
import { interval, map, catchError, of, Observable, shareReplay, startWith, Subscription, switchMap } from 'rxjs';
import { SystemService } from '../../services/system.service';
import { WebsocketService } from '../../services/web-socket.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { NbToastrService } from '@nebular/theme';

@Component({
  selector: 'app-logs',
  templateUrl: './system.component.html',
  styleUrl: './system.component.scss'
})
export class SystemComponent implements OnDestroy, AfterViewChecked {

  @ViewChild('scrollContainer') private scrollContainer!: ElementRef;
  public info$: Observable<ISystemInfo>;

  public logs: string[] = [];

  private websocketSubscription?: Subscription;

  public showLogs = false;

  public stopScroll: boolean = false;

  constructor(
    private websocketService: WebsocketService,
    private toastrService: NbToastrService,
    private systemService: SystemService
  ) {


    this.info$ = interval(5000).pipe(
      startWith(() => this.systemService.getInfo(0)),
      switchMap(() => {
        return this.systemService.getInfo(0)
      }),
      map(info => {
        info.power = parseFloat(info.power.toFixed(1))
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));
        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));
        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );


  }
  ngOnDestroy(): void {
    this.websocketSubscription?.unsubscribe();
  }
  public toggleLogs() {
    this.showLogs = !this.showLogs;

    if (this.showLogs) {
      this.websocketSubscription = this.websocketService.ws$.subscribe({
        next: (val) => {
          this.logs.push(val);
          if (this.logs.length > 256) {
            this.logs.shift();
          }
        }
      })
    } else {
      this.websocketSubscription?.unsubscribe();
    }
  }

  ngAfterViewChecked(): void {
    if(this.stopScroll == true){
      return;
    }
    if (this.scrollContainer?.nativeElement != null) {
      this.scrollContainer.nativeElement.scrollTo({ left: 0, top: this.scrollContainer.nativeElement.scrollHeight, behavior: 'smooth' });
    }
  }

  public restart() {
    this.systemService.restart().pipe(
      catchError(error => {
        this.toastrService.danger(`Failed to restart Device`, 'Error');
        return of(null);
      })
    ).subscribe(res => {
      if (res !== null) {
        this.toastrService.success(`Device restarted`, 'Success');
      }
    });
  }

}
