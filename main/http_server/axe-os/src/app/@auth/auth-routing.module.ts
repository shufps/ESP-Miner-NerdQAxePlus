import { NgxLoginComponent } from './login/login.component';
import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { NbAuthComponent } from '@nebular/auth';
import { NgxResetPasswordComponent } from './reset-password/reset-password.component';
import { NgxAuthComponent } from './ngx-auth/ngx-auth.component';

export const routes: Routes = [
  // .. here goes our components routes
  {
    path: '',
    component: NgxAuthComponent,
    children: [
        {
          path: '',
          redirectTo: 'login',
          pathMatch: 'full',
        },
        {
          path: 'login',
          component: NgxLoginComponent, // <---
        },
        {
          path: 'reset-password',
          component: NgxResetPasswordComponent, // <---
        },
      ],  // <---
  },

];

@NgModule({
  imports: [RouterModule.forChild(routes)],
  exports: [RouterModule],
})
export class NgxAuthRoutingModule {
}