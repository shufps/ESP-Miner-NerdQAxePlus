import { Component, OnDestroy, OnInit } from '@angular/core';
import { NbMediaBreakpointsService, NbMenuService, NbSidebarService, NbThemeService } from '@nebular/theme';

import { LayoutService } from '../../../@core/utils.ts';
import { SystemService } from '../../../services/system.service';
import { map, takeUntil } from 'rxjs/operators';
import { Subject, Observable } from 'rxjs';

@Component({
  selector: 'ngx-header',
  styleUrls: ['./header.component.scss'],
  templateUrl: './header.component.html',
})
export class HeaderComponent implements OnInit, OnDestroy {

  private destroy$: Subject<void> = new Subject<void>();
  userPictureOnly: boolean = false;
  user: any;
  private sidebarCollapsed: boolean = false;

  themes = [
    {
      value: 'cosmic',
      name: 'Default',
    },
    {
      value: 'default',
      name: 'Light',
    },
    {
      value: 'dark',
      name: 'Dark',
    },
  ];

  currentTheme = 'cosmic'; // Default theme if none is found in localStorage
  logoPath: string = ''; // Dynamically set logo path
  deviceModel: string = 'default'; // Fallback device model

  userMenu = [{ title: 'Profile' }, { title: 'Log out' }];

  info$: Observable<any>; // Device info observable

  constructor(
    private sidebarService: NbSidebarService,
    private menuService: NbMenuService,
    private themeService: NbThemeService,
    private layoutService: LayoutService,
    private breakpointService: NbMediaBreakpointsService,
    private infoService: SystemService // Inject InfoService
  ) {}

  ngOnInit() {
    this.loadThemeFromLocalStorage();
    this.loadSidebarStateFromLocalStorage();

    const { xl } = this.breakpointService.getBreakpointsMap();
    this.themeService
      .onMediaQueryChange()
      .pipe(
        map(([, currentBreakpoint]) => currentBreakpoint.width < xl),
        takeUntil(this.destroy$),
      )
      .subscribe((isLessThanXl: boolean) => (this.userPictureOnly = isLessThanXl));

    this.themeService
      .onThemeChange()
      .pipe(
        map(({ name }) => name),
        takeUntil(this.destroy$),
      )
      .subscribe((themeName) => {
        this.currentTheme = themeName;
        this.updateLogo();
        this.saveThemeToLocalStorage(themeName);
      });

    // Fetch device info
    this.infoService.getInfo(0).subscribe(info => {
      if (info && info.deviceModel) {
        this.deviceModel = info.deviceModel.replace('γ', 'Gamma');
      }
      this.updateLogo();
    });
  }

  ngOnDestroy() {
    this.destroy$.next();
    this.destroy$.complete();
  }

  changeTheme(themeName: string) {
    this.themeService.changeTheme(themeName);
    this.saveThemeToLocalStorage(themeName);
    this.updateLogo();
  }

  toggleSidebar(): boolean {
    if (this.sidebarCollapsed) {
      this.sidebarService.expand('menu-sidebar');
      this.sidebarCollapsed = false;
    } else {
      this.sidebarService.compact('menu-sidebar');
      this.sidebarCollapsed = true;
    }

    this.layoutService.changeLayoutSize();

    // Save the new state
    localStorage.setItem('sidebarCollapsed', this.sidebarCollapsed ? 'true' : 'false');

    return false;
  }

  navigateHome() {
    this.menuService.navigateHome();
    return false;
  }

  private updateLogo() {
    const logoVariant = this.currentTheme === 'default' ? 'light' : 'dark';
    this.logoPath = `/assets/${this.deviceModel}_${logoVariant}.png`;
  }

  private loadThemeFromLocalStorage() {
    const savedTheme = localStorage.getItem('selectedTheme');
    if (savedTheme && this.themes.some(t => t.value === savedTheme)) {
      this.currentTheme = savedTheme;
    } else {
      this.currentTheme = 'cosmic'; // Default theme
    }
    this.themeService.changeTheme(this.currentTheme);
    this.updateLogo();
  }

  private saveThemeToLocalStorage(theme: string) {
    localStorage.setItem('selectedTheme', theme);
  }

  private loadSidebarStateFromLocalStorage() {
    const sidebarCollapsed = localStorage.getItem('sidebarCollapsed');
    if (sidebarCollapsed === 'true') {
      this.sidebarCollapsed = true;
      // Compact the sidebar if it was collapsed before (shows icons only)
      setTimeout(() => {
        this.sidebarService.compact('menu-sidebar');
      }, 100);
    } else {
      this.sidebarCollapsed = false;
    }
  }
}
