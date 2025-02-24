import { Component, OnInit, Output, EventEmitter } from '@angular/core';
import { LocalStorageService } from '../../services/local-storage.service';

@Component({
    selector: 'app-advanced-toggle',
    templateUrl: './advanced-toggle.component.html',
    styleUrls: ['./advanced-toggle.component.css']
})
export class AdvancedToggleComponent implements OnInit {
    @Output() advancedToggled = new EventEmitter<boolean>();

    isAdvanced = false;
    private storageKey = 'advanced-mode';
    private storageExpiryKey = 'advanced-mode-expiry';

    constructor(private localStorageService: LocalStorageService) { }

    ngOnInit() {
        this.loadState();
        this.advancedToggled.emit(this.isAdvanced); // Emit the initial state

    }

    toggleAdvanced() {
        this.isAdvanced = !this.isAdvanced;
        this.advancedToggled.emit(this.isAdvanced);
        this.saveState();
    }

    private saveState() {
        const expiryTime = Date.now() + 24 * 60 * 60 * 1000; // 1 day in milliseconds
        this.localStorageService.setBool(this.storageKey, this.isAdvanced);
        this.localStorageService.setNumber(this.storageExpiryKey, expiryTime);
    }

    private loadState() {
        const expiry = this.localStorageService.getNumber(this.storageExpiryKey);
        if (expiry && Date.now() > expiry) {
            // Expired -> reset to false
            this.localStorageService.setBool(this.storageKey, false);
            this.localStorageService.setNumber(this.storageExpiryKey, 0);
            this.isAdvanced = false;
        } else {
            this.isAdvanced = this.localStorageService.getBool(this.storageKey);
        }
    }
}
