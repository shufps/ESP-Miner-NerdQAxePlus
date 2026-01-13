import { Component } from '@angular/core';
import { ExperimentalDashboardService } from '../../services/experimental-dashboard.service';

@Component({
  selector: 'app-home-shell',
  templateUrl: './home-shell.component.html',
})
export class HomeShellComponent {
  constructor(public experimental: ExperimentalDashboardService) {}
}
