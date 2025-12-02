import { Component, Input } from '@angular/core';
import { LocalStorageService } from 'src/app/services/local-storage.service';

/**
 * Reusable dismissible banner with "Don't show again" persistence.
 * Stores a boolean flag in localStorage under `storageKey`.
 */
@Component({
  selector: 'app-alert-banner',
  templateUrl: './alert-banner.component.html',
  styleUrls: ['./alert-banner.component.scss'],
})
export class AlertBannerComponent {
  /** LocalStorage key to persist dismissal */
  @Input() storageKey!: string;

  /** Nebular status: 'danger' | 'warning' | 'info' | 'success' | 'basic' */
  @Input() status: 'danger' | 'warning' | 'info' | 'success' | 'basic' = 'danger';

  /** Optional title and message (already translated strings or raw text) */
  @Input() title = '';
  @Input() message = '';

  /** Initial collapsed state is driven by storage */
  hidden = false;

  /** Checkbox state for "Don't show again" */
  dontShowAgain = false;

  constructor(private ls: LocalStorageService) {
    // no-op
  }

  ngOnInit(): void {
    // Hide if previously dismissed
    if (!this.storageKey) {
      // Fail safe: without a key, never hide by default
      return;
    }
    this.hidden = this.ls.getBool(this.storageKey) === true;
  }

  /** Close button clicked */
  dismiss(): void {
    if (this.dontShowAgain && this.storageKey) {
      this.ls.setBool(this.storageKey, true);
    }
    this.hidden = true;
  }
}
