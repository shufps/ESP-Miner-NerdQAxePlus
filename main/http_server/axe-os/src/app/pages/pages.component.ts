import { Component, OnDestroy, OnInit } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { TranslateService } from '@ngx-translate/core';
import { NbMenuItem } from '@nebular/theme';
import { catchError } from 'rxjs/operators';
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

    constructor(private translateService: TranslateService, private http: HttpClient) {}

    ngOnInit(): void {
        this.http.get<IIdentifyV2>('/api/v2/identify').pipe(
            catchError(() => of(null))
        ).subscribe(info => {
            this.canEnabled = info?.can?.enabled === true;
            this.buildMenu();
        });

        this.langSub = this.translateService.onLangChange.subscribe(() => {
            this.buildMenu();
        });
    }

    ngOnDestroy(): void {
        this.langSub?.unsubscribe();
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
    }
}
