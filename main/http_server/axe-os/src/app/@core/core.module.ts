import { CommonModule } from '@angular/common';
import {
  ModuleWithProviders,
  NgModule,
  Optional,
  SkipSelf,
} from '@angular/core';
import {
  NbAuthJWTToken,
  NbAuthModule,
  NbPasswordAuthStrategy,
} from '@nebular/auth';
import { NbRoleProvider, NbSecurityModule } from '@nebular/security';

import { throwIfAlreadyLoaded } from './module-import-guard';
import {
  NavigationService,
  LayoutService,
  StateService,
  RoleProvider,
  StorageService,
} from './utils.ts';

export const NB_CORE_PROVIDERS = [
  { provide: NbRoleProvider, useClass: RoleProvider }, // provide the class
  ...(NbAuthModule.forRoot({
    strategies: [
      NbPasswordAuthStrategy.setup({
        name: 'email',
        baseEndpoint: 'api/v1',
        login: {
          endpoint: '/auth/sign-in',
          method: 'post',
        },
        register: {
          endpoint: '/auth/sign-up',
          method: 'post',
        },
        logout: {
          endpoint: '/auth/sign-out',
          redirect: {
            success: '/auth/login',
            failure: null,
          },
          method: 'post',
        },
        requestPass: {
          endpoint: '/auth/request-pass',
          method: 'post',
        },
        resetPass: {
          endpoint: '/auth/reset-pass',
          method: 'post',
        },
        refreshToken: {
          endpoint: '/auth/refresh-token',
          method: 'post',
          redirect: {
            success: '/auth/login',
            failure: null,
          },
        },
        token: {
          class: NbAuthJWTToken,
        },
      }),
    ],
    forms: {
      login: {
        strategy: 'email',
      },
      register: {},
    },
  }).providers || []),
  LayoutService,
  StateService,
  StorageService,
  NavigationService,
];

@NgModule({
  imports: [
    CommonModule,
    NbSecurityModule.forRoot({
      accessControl: {
        guest: {
          edit: ['role_guest'],
        },
        user: {
          parent: 'guest',
          edit: ['role_user'],
        },
        reseller: {
          parent: 'user',
          edit: ['role_reseller'],
        },
        admin: {
          parent: 'reseller',
          edit: ['role_admin', 'ftp_credential_role', 'user_coupon'],
        },
      },
    }),
  ],
  exports: [NbAuthModule],
  declarations: [],
})
export class CoreModule {
  constructor(@Optional() @SkipSelf() parentModule: CoreModule) {
    throwIfAlreadyLoaded(parentModule, 'CoreModule');
  }

  static forRoot(): ModuleWithProviders<CoreModule> {
    return {
      ngModule: CoreModule,
      providers: [...NB_CORE_PROVIDERS],
    };
  }
}
