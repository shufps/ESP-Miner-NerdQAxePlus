import { Component, OnDestroy, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { NbMenuItem } from '@nebular/theme';

@Component({
    selector: 'ngx-pages',
    styleUrls: ['pages.component.scss'],
    templateUrl: './pages.component.html',
  })
  export class PagesComponent implements OnInit, OnDestroy{
    menu: NbMenuItem[] = [];

    constructor(private translateService: TranslateService)
    {

    }

    ngOnInit(): void {
        this.buildMenu();

        // Listen for language changes
        this.translateService.onLangChange.subscribe(() => {
            this.buildMenu();
        });
    }

    ngOnDestroy(): void {

    }

    private buildMenu(): void {
        this.menu = [
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
                title: this.translateService.instant('NAVIGATION.SYSTEM'),
                icon: 'menu-outline',
                link: '/pages/system',
            },
        ];
    }
}