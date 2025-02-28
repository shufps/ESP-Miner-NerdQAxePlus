import { NgxLoginComponent } from './login/login.component';
import { CommonModule } from '@angular/common';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { RouterModule } from '@angular/router';


import { NgxAuthRoutingModule } from './auth-routing.module';
import { NbAuthModule } from '@nebular/auth';
import { 
  NbAlertModule,
  NbButtonModule,
  NbCardModule,
  NbCheckboxModule,
  NbInputModule,
  NbLayoutModule
} from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { NgxResetPasswordComponent } from './reset-password/reset-password.component';
import { NgxAuthComponent } from './ngx-auth/ngx-auth.component';


@NgModule({
  imports: [
    CommonModule,
    NbCardModule,
    NbLayoutModule,
    FormsModule,
    RouterModule,
    NbAlertModule,
    NbInputModule,
    NbButtonModule,
    NbCheckboxModule,
    NbAlertModule,
    TranslateModule,
    NgxAuthRoutingModule,
    TranslateModule,
    NbAuthModule,

  ],
  declarations: [
    NgxAuthComponent,
    NgxLoginComponent,
    NgxResetPasswordComponent
  ],
})
export class NgxAuthModule {
}