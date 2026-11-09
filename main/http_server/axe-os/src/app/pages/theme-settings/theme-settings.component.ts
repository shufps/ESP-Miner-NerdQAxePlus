import { Component, OnInit } from '@angular/core';
import { NbThemeService } from '@nebular/theme';
import { GaiaAccentService, GaiaAccentPreset } from '../../services/gaia-accent.service';

@Component({
  selector: 'app-theme-settings',
  template: `
    <nb-card>
      <nb-card-header>Theme</nb-card-header>
      <nb-card-body>
        <h5>Appearance</h5>
        <p>Select your theme. The choice is saved in this browser.</p>
        <div class="theme-options">
          <button nbButton type="button" *ngFor="let item of themes"
            [status]="currentTheme === item.value ? 'primary' : 'basic'"
            (click)="setTheme(item.value)">{{ item.label }}</button>
        </div>
        <ng-container *ngIf="currentTheme === 'gaia'">
          <h5>Gaia appearance</h5>
          <p>Choose Gaia Gold or Gaia-Dark with GitHub-inspired backgrounds and cards.</p>
          <div class="theme-options">
            <button class="preset" type="button" *ngFor="let preset of accent.presets"
              [class.selected]="accent.selected === preset.id"
              [attr.aria-pressed]="accent.selected === preset.id"
              (click)="setPreset(preset.id)">
              <span class="swatch" [style.background]="preset.color"></span>
              {{ preset.label }}
            </button>
          </div>
          <p class="legend">
            <span style="color:#39c5cf">● ASIC temperature</span>
            <span style="color:#3fbf9b">● VR temperature</span>
            <span style="color:#f0584b">● ASIC ≥ 60 °C</span>
          </p>
        </ng-container>
      </nb-card-body>
    </nb-card>
  `,
  styles: [`
    .theme-options { display:flex; flex-wrap:wrap; gap:.75rem; margin:1rem 0 1.5rem; }
    .preset { display:flex; align-items:center; gap:.65rem; padding:.75rem 1rem;
      border:1px solid var(--border-basic-color-4); border-radius:.6rem;
      color:var(--text-basic-color); background:var(--background-basic-color-2);
      cursor:pointer; font:inherit; }
    .preset.selected { border-color:var(--color-primary-500); box-shadow:0 0 0 1px var(--color-primary-500); }
    .swatch { display:inline-block; width:1.5rem; height:1.5rem; border-radius:50%; }
    .legend { display:flex; gap:1rem; flex-wrap:wrap; }
  `]
})
export class ThemeSettingsComponent implements OnInit {
  currentTheme = 'cosmic';
  readonly themes = [
    { value: 'cosmic', label: 'Default' },
    { value: 'default', label: 'Light' },
    { value: 'dark', label: 'Dark' },
    { value: 'gaia', label: 'Gaia' },
  ];
  constructor(private themeService: NbThemeService, public accent: GaiaAccentService) {}
  ngOnInit(): void {
    this.currentTheme = this.themeService.currentTheme;
    this.accent.apply();
  }
  setTheme(theme: string): void {
    this.currentTheme = theme;
    this.themeService.changeTheme(theme);
    localStorage.setItem('selectedTheme', theme);
    setTimeout(() => this.accent.apply(), 0);
  }
  setPreset(preset: GaiaAccentPreset): void {
    this.accent.select(preset);
    window.location.reload();
  }
}
