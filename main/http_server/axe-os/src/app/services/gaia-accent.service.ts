import { Injectable } from '@angular/core';

export type GaiaAccentPreset = 'gold' | 'blue';

@Injectable({ providedIn: 'root' })
export class GaiaAccentService {
  readonly storageKey = 'gaiaAccentPreset';
  readonly presets = [
    { id: 'gold' as GaiaAccentPreset, label: 'Gaia Gold', color: '#e8a23c' },
    { id: 'blue' as GaiaAccentPreset, label: 'Gaia Blue', color: '#398bd0' },
  ];

  get selected(): GaiaAccentPreset {
    return localStorage.getItem(this.storageKey) === 'blue' ? 'blue' : 'gold';
  }

  get color(): string {
    return this.selected === 'blue' ? '#398bd0' : '#e8a23c';
  }

  select(preset: GaiaAccentPreset): void {
    localStorage.setItem(this.storageKey, preset);
    this.apply();
  }

  apply(): void {
    const blue = this.selected === 'blue';
    const colors = blue
      ? ['#eaf4fd', '#d2e8f9', '#a5d1f1', '#73b4e5', '#4e9bd8', '#398bd0', '#2875b5', '#1d5c91', '#164671']
      : [];
    document.documentElement.style.setProperty('--gaia-accent', this.color);
    document.querySelectorAll<HTMLElement>('.nb-theme-gaia').forEach(el => {
      el.style.setProperty('--gaia-accent', this.color);
      for (let i = 0; i < 9; i++) {
        const name = `--color-primary-${(i + 1) * 100}`;
        if (blue) el.style.setProperty(name, colors[i]);
        else el.style.removeProperty(name);
      }
    });
  }
}
