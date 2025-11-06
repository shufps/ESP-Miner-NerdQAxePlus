import { Component, OnDestroy, OnInit } from '@angular/core';
import { NbMediaBreakpointsService, NbMenuService, NbSidebarService, NbThemeService } from '@nebular/theme';
import { LayoutService } from '../../../@core/utils.ts';
import { SystemService } from '../../../services/system.service';
import { map, takeUntil } from 'rxjs/operators';
import { Subject, Observable } from 'rxjs';
import { ISystemInfo } from 'src/app/models/ISystemInfo.js';

@Component({
  selector: 'ngx-header',
  styleUrls: ['./header.component.scss'],
  templateUrl: './header.component.html',
})
export class HeaderComponent implements OnInit, OnDestroy {

  private destroy$: Subject<void> = new Subject<void>();
  private bootstrapping = true; // Prevents theme events from clobbering the logo while initializing

  userPictureOnly = false;
  user: any;

  // Sidebar state: expanded → compact → collapsed → expanded
  private sidebarState: 'expanded' | 'compact' | 'collapsed' = 'expanded';

  themes = [
    { value: 'cosmic',  name: 'Default' },
    { value: 'default', name: 'Light' },
    { value: 'dark',    name: 'Dark' },
  ];

  currentTheme = 'cosmic';  // Default theme if none is found in localStorage
  logoPath: string = '';    // Resolved logo path for the template
  deviceModel: string = 'default'; // Fallback device model

  private logoBaseName: string | null = null;

  // Transparent 16x16 placeholder (must exist in /assets)
  private static readonly PLACEHOLDER_LOGO = '/assets/default_dark.png';

  userMenu = [{ title: 'Profile' }, { title: 'Log out' }];
  info$: Observable<ISystemInfo>;

  constructor(
    private sidebarService: NbSidebarService,
    private menuService: NbMenuService,
    private themeService: NbThemeService,
    private layoutService: LayoutService,
    private breakpointService: NbMediaBreakpointsService,
    private infoService: SystemService
  ) {
    this.setPlaceholderLogo();
  }

  ngOnInit() {
    this.loadSidebarStateFromLocalStorage();

    // Responsive behavior
    const { xl } = this.breakpointService.getBreakpointsMap();
    this.themeService.onMediaQueryChange()
      .pipe(
        map(([, bp]) => bp.width < xl),
        takeUntil(this.destroy$),
      )
      .subscribe(isLessThanXl => this.userPictureOnly = isLessThanXl);

    // Persist theme changes - but not during bootstrapping
    this.themeService.onThemeChange()
      .pipe(
        map(({ name }) => name),
        takeUntil(this.destroy$),
      )
      .subscribe(themeName => {
        this.currentTheme = themeName;

        if (!this.bootstrapping) {
          this.updateLogo();
          this.saveThemeToLocalStorage(themeName);
        }
      });

    // 1) If available, use saved theme
    const savedTheme = localStorage.getItem('selectedTheme');
    if (savedTheme && this.isThemeValid(savedTheme)) {
      this.applyTheme(savedTheme); // no saving here
    }

    // 2) Always load backend info (device name/model + default theme)
    this.infoService.getInfo(0).subscribe(info => {
      if (info?.deviceModel) {
        // Replace gamma symbol with "Gamma" to match asset filenames if needed
        this.deviceModel = String(info.deviceModel).replace('γ', 'Gamma');
        this.logoBaseName = String(info.deviceModel).trim();
      }

      // Prefer backend default theme if no valid saved theme is present
      const backendTheme = info?.defaultTheme ?? '';
      if ((!savedTheme || !this.isThemeValid(savedTheme)) && this.isThemeValid(backendTheme)) {
        this.applyTheme(backendTheme);
      } else if (!savedTheme) {
        // 3) Final fallback
        this.applyTheme('cosmic');
      }

      // Finalize bootstrap: now we can safely set logo and persist theme (once)
      this.bootstrapping = false;
      this.updateLogo();
      if (!savedTheme) {
        this.saveThemeToLocalStorage(this.currentTheme);
      }
    }, _err => {
      // Backend not responding, use fallback theme and try legacy logo scheme
      if (!savedTheme) {
        this.applyTheme('cosmic');
      }
      this.bootstrapping = false;
      this.updateLogo();
      if (!savedTheme) this.saveThemeToLocalStorage(this.currentTheme);
    });
  }

  changeTheme(themeName: string) {
    // Trigger Nebular theme change; saving happens in onThemeChange() after bootstrapping
    this.themeService.changeTheme(themeName);
    // Keep logo in sync (if bootstrapping is false this will be the effective update)
    this.updateLogo();
  }

  ngOnDestroy() {
    this.destroy$.next();
    this.destroy$.complete();
  }

  //  Sidebar 3-state toggle logic
  toggleSidebar(): boolean {
    const isMobile = window.innerWidth < 768;

    switch (this.sidebarState) {
      case 'expanded':
        if (isMobile) {
          this.sidebarService.collapse('menu-sidebar');
          this.sidebarState = 'collapsed';
        } else {
          this.sidebarService.compact('menu-sidebar');
          this.sidebarState = 'compact';
        }
        break;

      case 'compact':
        this.sidebarService.collapse('menu-sidebar');
        this.sidebarState = 'collapsed';
        break;

      case 'collapsed':
        this.sidebarService.expand('menu-sidebar');
        this.sidebarState = 'expanded';
        break;
    }

    this.layoutService.changeLayoutSize();

    // Save the new state
    localStorage.setItem('sidebarState', this.sidebarState);

    return false;
  }

  navigateHome() {
    this.menuService.navigateHome();
    return false;
  }

  private setPlaceholderLogo() {
    // Always set the placeholder, regardless of theme or device
    this.logoPath = HeaderComponent.PLACEHOLDER_LOGO;
  }

  private updateLogo() {
    // While bootstrapping, force the placeholder
    if (this.bootstrapping) {
      this.setPlaceholderLogo();
      return;
    }

    // Choose logo variant based on theme ("default" is the light theme in Nebular).
    const logoVariant = this.currentTheme === 'default' ? 'light' : 'dark';
    this.logoPath = `/assets/${this.deviceModel}_${logoVariant}.png`;
  }

  // --- Helpers for theme resolution ---

  /** Check if a theme value exists in the configured theme options. */
  private isThemeValid(value: string): boolean {
    return this.themes.some(t => t.value === value);
  }

  /** Apply theme through NbThemeService and keep local state. */
  private applyTheme(theme: string) {
    // Update current theme and broadcast to Nebular
    this.currentTheme = theme;
    this.themeService.changeTheme(theme);
    // Do not update the logo here; we call updateLogo() explicitly after bootstrapping
  }

  private saveThemeToLocalStorage(theme: string) {
    localStorage.setItem('selectedTheme', theme);
  }

  private loadSidebarStateFromLocalStorage() {
    const savedState = localStorage.getItem('sidebarState') as 'expanded' | 'compact' | 'collapsed' | null;
    // on mobile the default is collapsed, otherwise expanded
    const isMobile = window.innerWidth < 768;
    this.sidebarState = savedState ?? (isMobile ? 'collapsed' : 'expanded');
  }

  ngAfterViewInit() {
    this.applySidebarState();
  }

  private applySidebarState() {
    switch (this.sidebarState) {
      case 'compact':
        this.sidebarService.compact('menu-sidebar');
        break;
      case 'collapsed':
        this.sidebarService.collapse('menu-sidebar');
        break;
      default:
        this.sidebarService.expand('menu-sidebar');
    }
  }
}
