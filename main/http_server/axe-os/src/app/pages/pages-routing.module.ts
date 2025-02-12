import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

import { PagesComponent } from './pages.component';
import { NotFoundComponent } from '../@theme/components';
import { HomeComponent } from './home/home.component';
import { SettingsComponent } from './settings/settings.component';
import { InfluxdbComponent } from './influxdb/influxdb.component';
import { LogsComponent } from './logs/logs.component';
import { SwarmComponent } from './swarm/swarm.component';

const routes: Routes = [{
  path: '',
  component: PagesComponent,
  children: [
    {
      path: '',
      redirectTo: 'home',
      pathMatch: 'full',
    },
    {
      path: 'home',
      component: HomeComponent
    },
    {
      path: 'settings',
      component: SettingsComponent
    },
    {
      path: 'influxdb',
      component: InfluxdbComponent
    },
    {
      path: 'logs',
      component: LogsComponent
    },
    {
      path: 'swarm',
      component: SwarmComponent
    },
    {
      path: '**',
      component: NotFoundComponent,
    },

  ],
}];

@NgModule({
  imports: [RouterModule.forChild(routes)],
  exports: [RouterModule],
})
export class PagesRoutingModule {
}
