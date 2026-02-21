import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { HomeComponent } from './home.component';
import { NbCardModule, NbTooltipModule, NbBadgeModule, NbAlertModule, NbButtonModule, NbThemeModule, NbIconModule } from '@nebular/theme';
import { SystemService } from '../../services/system.service';
import { GaugeModule } from '../../components/gauge/gauge.module';
import { PipesModule} from '../../pipes/pipes.module';
import { TranslateModule } from '@ngx-translate/core';
import { AsicTempBarsModule } from 'src/app/components/asic-temp-bars/asic-temp-bars.module';

@NgModule({
  declarations: [
    HomeComponent,
  ],
  imports: [
    CommonModule,
    NbCardModule,
    NbButtonModule,
    NbIconModule,
    NbThemeModule,
    GaugeModule,
    PipesModule,
    NbBadgeModule,
    NbAlertModule,
    NbTooltipModule,
    TranslateModule,
    AsicTempBarsModule,
  ],
  providers: [
    SystemService
  ]
})
export class HomeModule {}
