import { Component } from '@angular/core';
import { map, startWith } from 'rxjs';
import { ExperimentalDashboardService } from '../../services/experimental-dashboard.service';

type HomeShellMode = 'loading' | 'normal' | 'experimental';

@Component({
  selector: 'app-home-shell',
  templateUrl: './home-shell.component.html',
})
export class HomeShellComponent {
  /**
   * Use an explicit mode instead of '*ngIf="enabled$ | async as enabled"' to avoid:
   * - "false" being treated as "loading" (blank page)
   * - rendering the normal component before enabled$ emits (style/DOM flash)
   */
  readonly mode$ = this.experimental.enabled$.pipe(
    map((enabled): HomeShellMode => (enabled ? 'experimental' : 'normal')),
    startWith('loading' as HomeShellMode),
  );

  constructor(public experimental: ExperimentalDashboardService) {}
}
