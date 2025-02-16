import { inject } from '@angular/core';
import {
  ActivatedRouteSnapshot,
  CanActivateFn,
  Router,
  RouterStateSnapshot,
  UrlTree,
} from '@angular/router';
import { NbAuthService, NbTokenService } from '@nebular/auth';
import { Observable, map } from 'rxjs';

export const AuthGuard: CanActivateFn = (
  route: ActivatedRouteSnapshot,
  state: RouterStateSnapshot
):
  | Observable<boolean | UrlTree>
  | Promise<boolean | UrlTree>
  | boolean
  | UrlTree => {
  const authService = inject(NbAuthService);
  const router = inject(Router);
  const tokenService = inject(NbTokenService);

  return authService.isAuthenticated().pipe(
    map((status) => {
      if (!status) {
        authService.logout('email');
        tokenService.clear();
        return router.createUrlTree(['/auth/login']);
      }
      return true;
    })
  );
};
