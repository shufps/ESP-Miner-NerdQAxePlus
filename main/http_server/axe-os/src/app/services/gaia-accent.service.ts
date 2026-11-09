import { Injectable } from '@angular/core';

export type GaiaAccentPreset = 'gold' | 'dark';

@Injectable({ providedIn: 'root' })
export class GaiaAccentService {
  readonly storageKey = 'gaiaAccentPreset';
  readonly presets = [
    { id: 'gold' as GaiaAccentPreset, label: 'Gaia Gold', color: '#e8a23c' },
    { id: 'dark' as GaiaAccentPreset, label: 'Gaia-Dark', color: '#2f81f7' },
  ];

  get selected(): GaiaAccentPreset {
    const stored = localStorage.getItem(this.storageKey);
    // Migrate the previous Gaia Blue selection without losing the preference.
    return stored === 'blue' || stored === 'dark' ? 'dark' : 'gold';
  }

  get color(): string {
    return this.selected === 'dark' ? '#2f81f7' : '#e8a23c';
  }

  select(preset: GaiaAccentPreset): void {
    localStorage.setItem(this.storageKey, preset);
    this.apply();
  }

  apply(): void {
    const dark = this.selected === 'dark';
    const palette: { [key: string]: string } = {
      '--gaia-accent': dark ? '#2f81f7' : '#e8a23c',
      '--gaia-card-accent': dark ? '#2f81f7' : '#e8a23c',
    };
    if (dark) {
      Object.assign(palette, {
        '--layout-background-color': '#010409',
        '--background-basic-color-1': '#0d1117',
        '--background-basic-color-2': '#161b22',
        '--background-basic-color-3': '#21262d',
        '--background-basic-color-4': '#30363d',
        '--card-background-color': '#0d1117',
        '--card-border-color': '#30363d',
        '--header-background-color': '#0d1117',
        '--sidebar-background-color': '#0d1117',
        '--select-background-color': '#161b22',
        '--border-basic-color-1': '#161b22',
        '--border-basic-color-2': '#21262d',
        '--border-basic-color-3': '#30363d',
        '--border-basic-color-4': '#484f58',
        '--border-basic-color-5': '#6e7681',
        '--text-basic-color': '#e6edf3',
        '--card-text-color': '#e6edf3',
        '--header-text-color': '#e6edf3',
        '--text-hint-color': '#8b949e',
        '--menu-text-color': '#c9d1d9',
        '--menu-item-icon-color': '#8b949e',
        '--menu-item-hover-background-color': 'rgba(47,129,247,.10)',
        '--menu-item-active-background-color': 'rgba(47,129,247,.16)',
        '--menu-item-active-text-color': '#2f81f7',
        '--menu-item-icon-active-color': '#2f81f7',
        '--sidebar-menu-item-highlight-color': '#2f81f7',
        '--color-primary-100': '#cae8ff',
        '--color-primary-200': '#a5d6ff',
        '--color-primary-300': '#79c0ff',
        '--color-primary-400': '#58a6ff',
        '--color-primary-500': '#2f81f7',
        '--color-primary-600': '#1f6feb',
        '--color-primary-700': '#1158c7',
        '--color-primary-800': '#0d419d',
        '--color-primary-900': '#0a3069',
      });
    }

    // Nebular attaches the theme class to a layout ancestor. Apply variables to
    // that same element so cards, menus and headers inherit the correct palette.
    document.querySelectorAll<HTMLElement>('.nb-theme-gaia').forEach(el => {
      el.classList.toggle('gaia-dark', dark);
      const old = (el as any).__gaiaPresetKeys as string[] || [];
      old.forEach(key => el.style.removeProperty(key));
      Object.keys(palette).forEach(key => el.style.setProperty(key, palette[key]));
      (el as any).__gaiaPresetKeys = Object.keys(palette);
    });
  }
}
