import { Component, OnDestroy, OnInit } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { NavigationEnd, Router } from '@angular/router';
import { TranslateService } from '@ngx-translate/core';
import { NbMenuItem } from '@nebular/theme';
import { catchError, filter } from 'rxjs/operators';
import { of, Subscription } from 'rxjs';
import { IIdentifyV2 } from '../models/IIdentifyV2';

@Component({
    selector: 'ngx-pages',
    styleUrls: ['pages.component.scss'],
    templateUrl: './pages.component.html',
  })
  export class PagesComponent implements OnInit, OnDestroy {
    menu: NbMenuItem[] = [];

    private canEnabled = false;
    private langSub?: Subscription;
    private routerSub?: Subscription;

    constructor(
        private translateService: TranslateService,
        private http: HttpClient,
        private router: Router,
    ) {}

    ngOnInit(): void {
        // Build the menu synchronously first so nb-menu initialises with real
        // items (its initial active-item detection only runs once, at init).
        this.buildMenu();

        this.http.get<IIdentifyV2>('/api/v2/identify').pipe(
            catchError(() => of(null))
        ).subscribe(info => {
            this.canEnabled = info?.can?.enabled === true;
            this.buildMenu();
        });

        this.langSub = this.translateService.onLangChange.subscribe(() => {
            this.buildMenu();
        });

        // nb-menu re-computes the active item on NavigationEnd, but because our
        // menu array is (re)built asynchronously after init, the highlight for
        // the current route can be lost. Re-mark the active item on navigation.
        this.routerSub = this.router.events.pipe(
            filter(e => e instanceof NavigationEnd),
        ).subscribe(() => this.markActiveItem());
    }

    ngOnDestroy(): void {
        this.langSub?.unsubscribe();
        this.routerSub?.unsubscribe();
    }

    /**
     * Marks the menu item matching the current URL as `selected` so Nebular adds
     * the `.active` class (used by the Gaia theme for the highlighted item).
     * Works with HashLocationStrategy: `router.url` is the path without the hash.
     */
    private markActiveItem(): void {
        const url = (this.router.url || '').split(/[?#]/)[0];
        for (const item of this.menu) {
            item.selected = !!item.link && (url === item.link || url.startsWith(item.link + '/'));
        }
    }

    private buildMenu(): void {
        const items: NbMenuItem[] = [
            {
                title: this.translateService.instant('NAVIGATION.DASHBOARD'),
                icon: 'home-outline',
                link: '/pages/home',
                home: true,
            },
            {
                title: this.translateService.instant('NAVIGATION.SWARM'),
                icon: 'share-outline',
                link: '/pages/swarm',
            },
            {
                title: this.translateService.instant('NAVIGATION.SETTINGS'),
                icon: 'settings-2-outline',
                link: '/pages/settings',
            },
            {
                title: this.translateService.instant('NAVIGATION.INFLUXDB'),
                icon: 'archive',
                link: '/pages/influxdb',
            },
            {
                title: this.translateService.instant('NAVIGATION.ALERTS'),
                icon: 'bell-outline',
                link: '/pages/alert',
            },
            {
                title: this.translateService.instant('NAVIGATION.SECURITY'),
                icon: 'shield-outline',
                link: '/pages/security',
            },
        ];

        if (this.canEnabled) {
            items.push({
                title: 'CAN Fleet',
                icon: 'share-outline',
                link: '/pages/can-fleet',
            });
        }

        items.push({
            title: this.translateService.instant('NAVIGATION.SYSTEM'),
            icon: 'menu-outline',
            link: '/pages/system',
        });

        this.menu = items;
        this.markActiveItem();
    }
}
