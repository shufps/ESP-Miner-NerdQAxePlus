import { ChangeDetectionStrategy, Component } from '@angular/core';
import { NbThemeService } from '@nebular/theme';
import { distinctUntilChanged, map, startWith } from 'rxjs';

type HomeShellMode = 'normal' | 'gaia';

/**
 * Route target for /home: renders the dashboard that belongs to the active theme.
 * The "Gaia" theme gets its own dashboard (HomeGaiaComponent), every other theme
 * the default one (HomeComponent). Switching the theme swaps the component.
 */
@Component({
  selector: 'app-home-shell',
  templateUrl: './home-shell.component.html',
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class HomeShellComponent {
  readonly mode$ = this.themeService.onThemeChange().pipe(
    map((theme): string => theme?.name),
    startWith(this.themeService.currentTheme),
    map((name): HomeShellMode => (name === 'gaia' ? 'gaia' : 'normal')),
    distinctUntilChanged(),
  );

  constructor(private themeService: NbThemeService) {}
}
