import { NgModule } from '@angular/core';
import { NbAlertModule, NbCardModule, NbIconModule, NbMenuModule } from '@nebular/theme';

import { ThemeModule } from '../@theme/theme.module';
import { PagesComponent } from './pages.component';

import { PagesRoutingModule } from './pages-routing.module';
import { TranslateModule } from '@ngx-translate/core';
import { HomeComponent } from './home/home.component';
import { GaugeModule } from './home/gauge/gauge.module';
import { SettingsModule } from './settings/settings.module';
import { EditModule } from './edit/edit.module';
import { InfluxDBModule } from './influxdb/influxdb.module';
import { SystemModule } from './system/system.module';
import { SwarmModule } from './swarm/swarm.module';

import { PipesModule} from '../pipes/pipes.module';

@NgModule({
  imports: [
    PagesRoutingModule,
    ThemeModule,
    NbMenuModule,
    TranslateModule,
    NbCardModule,
    NbAlertModule,
    NbIconModule,
    GaugeModule,
    SettingsModule,
    EditModule,
    InfluxDBModule,
    SystemModule,
    SwarmModule,
    PipesModule
    ],
  declarations: [
    PagesComponent,
    HomeComponent,
  ],
})
export class PagesModule {
}
