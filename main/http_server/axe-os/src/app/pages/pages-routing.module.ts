import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

import { PagesComponent } from './pages.component';
import { NotFoundComponent } from '../@theme/components';
import { HomeComponent } from './home/home.component';
import { SettingsComponent } from './settings/settings.component';
import { InfluxdbComponent } from './influxdb/influxdb.component';
import { SystemComponent } from './system/system.component';
import { SwarmComponent } from './swarm/swarm.component';
import { AlertComponent } from './alert/alert.component';
import { SecurityComponent } from './security/security.component';

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
      path: 'system',
      component: SystemComponent
    },
    {
      path: 'swarm',
      component: SwarmComponent
    },
    {
      path: 'alert',
      component: AlertComponent
    },
    {
      path: 'security',
      component: SecurityComponent
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
