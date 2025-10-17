import { Component, Input, OnDestroy } from '@angular/core';
import { NbDialogRef } from '@nebular/theme';
import { interval, map, startWith, Subscription } from 'rxjs';

@Component({
  selector: 'app-otp-dialog',
  templateUrl: './otp-dialog.component.html',
  styleUrls: ['./otp-dialog.component.scss'],
})
export class OtpDialogComponent implements OnDestroy {
  @Input() title?: string;
  @Input() hint?: string;
  @Input() periodSec?: number; // default 30

  code = '';
  //remaining = 30;
  private sub?: Subscription;

  constructor(private ref: NbDialogRef<OtpDialogComponent>) {
/*
    // countdown init happens in ngOnInit-like pattern below
    const period = this.periodSec ?? 30;
    this.sub = interval(1000).pipe(
      startWith(0),
      map(() => {
        const p = this.periodSec ?? 30;
        const now = Math.floor(Date.now() / 1000);
        return p - (now % p);
      }),
    ).subscribe(v => this.remaining = v);
*/
  }

  onInput() {
    this.code = (this.code || '').replace(/[^0-9]/g, '').slice(0, 6);
  }
  submit() { if (this.code.length === 6) this.ref.close(this.code); }
  cancel() { this.ref.close(null); }
  onKeydown(e: KeyboardEvent) { if (e.key === 'Enter') this.submit(); if (e.key === 'Escape') this.cancel(); }

  ngOnDestroy() { this.sub?.unsubscribe(); }
}
