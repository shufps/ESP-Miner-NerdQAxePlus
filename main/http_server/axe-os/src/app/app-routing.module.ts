import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { AuthGuard } from './app-auth-guard.service';

const routes: Routes = [
  {
    path: 'pages',
    //canActivate: [AuthGuard], <-- Remove comment to enable auth guard. Remeber to setup NbAuthServiceModule in @core accordingly with yuor backend
    loadChildren: () => import('./pages/pages.module')
      .then(m => m.PagesModule),
  },
  {
    path: 'auth',
    loadChildren: () => import('./@auth/auth.module')
    .then(m => m.NgxAuthModule),
  },
  { path: '', redirectTo: 'pages', pathMatch: 'prefix'},
  { path: '**', redirectTo: 'pages' },
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule]
})
export class AppRoutingModule { }
