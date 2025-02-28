import { Component, OnDestroy, OnInit } from '@angular/core';
import { MENU_ITEMS } from './pages-menu';

@Component({
    selector: 'ngx-pages',
    styleUrls: ['pages.component.scss'],
    templateUrl: './pages.component.html',
  })
  export class PagesComponent implements OnInit, OnDestroy{
    menu = MENU_ITEMS;

    constructor()
    {

    }

    ngOnInit(): void {
        
    }

    ngOnDestroy(): void {
        
    }
}