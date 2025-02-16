import { Injectable } from '@angular/core';
import { map } from 'rxjs/operators';

import { NbAuthService, NbAuthToken } from '@nebular/auth';
import { NbRoleProvider } from '@nebular/security';
import { Observable } from 'rxjs';

@Injectable()
export class RoleProvider implements NbRoleProvider {

  constructor(private authService: NbAuthService) {
  }

  getRole(): Observable<string> {
    return this.authService.onTokenChange()
      .pipe(
        map((token: NbAuthToken) => {
          return token.isValid() ? token.getPayload()['ruolo'][0] : 'guest';
        }),
      );
  }
}
