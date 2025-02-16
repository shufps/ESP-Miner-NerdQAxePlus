import { CommonModule } from "@angular/common";
import { ModuleWithProviders, NgModule } from "@angular/core";
import { CORPORATE_THEME } from "./styles/theme.corporate";
import {
    NbActionsModule,
    NbLayoutModule,
    NbMenuModule,
    NbSearchModule,
    NbSidebarModule,
    NbUserModule,
    NbContextMenuModule,
    NbButtonModule,
    NbSelectModule,
    NbIconModule,
    NbThemeModule,
    NbCardModule,
    NbAlertModule,
  } from '@nebular/theme';
import { NbSecurityModule } from "@nebular/security";
import { NbEvaIconsModule } from "@nebular/eva-icons";

import {
    FooterComponent,
    HeaderComponent,
    HelpComponent,
    GoBackComponent,
    NotFoundComponent
  } from './components';

  import {
    WaitDialogComponent,
    ConfirmDialogComponent,
  } from './dialogs';

  import {
    CapitalizePipe,
    PluralPipe,
    RoundPipe,
    TimingPipe,
    NumberWithCommasPipe,
    ArrayToStringPipe,
    NumberToStringPipe,
    SafePipe,
    StringToNumberPipe,
    SplitPipe,
    SecurePipe,
    RemoveExtension,
    SanitizeHtmlPipe,
    CastPipe
  } from './pipes';

  import {
    OneColumnLayoutComponent,
    ThreeColumnsLayoutComponent,
    TwoColumnsLayoutComponent,
  } from './layouts';
import { TranslateModule } from "@ngx-translate/core";
import { CustomDirectivesModule } from "./directives/directives.modules";

const NB_MODULES : any= [
    NbLayoutModule,
    NbMenuModule,
    NbUserModule,
    NbActionsModule,
    NbButtonModule,
    NbSearchModule,
    NbSidebarModule,
    NbContextMenuModule,
    NbAlertModule,
    NbCardModule,
    NbSecurityModule,
    NbButtonModule,
    NbSelectModule,
    NbIconModule,
    NbEvaIconsModule,
  ];
  const COMPONENTS : any = [
    HeaderComponent,
    FooterComponent,
    OneColumnLayoutComponent,
    ThreeColumnsLayoutComponent,
    TwoColumnsLayoutComponent,
    HelpComponent,
    ConfirmDialogComponent,
    WaitDialogComponent,
    NotFoundComponent,
    GoBackComponent
  ];

const PIPES = [
  CapitalizePipe,
  PluralPipe,
  RoundPipe,
  TimingPipe,
  ArrayToStringPipe,
  NumberWithCommasPipe,
  NumberToStringPipe,
  StringToNumberPipe,
  SplitPipe,
  SecurePipe,
  RemoveExtension,
  SanitizeHtmlPipe,
  CastPipe,
  SafePipe
];

@NgModule({
    imports: [CommonModule, TranslateModule, ...NB_MODULES, CustomDirectivesModule],
    exports: [CommonModule, ...PIPES, ...COMPONENTS, CustomDirectivesModule],
    declarations: [...COMPONENTS, ...PIPES],
  })
  export class ThemeModule {
    static forRoot(): ModuleWithProviders<ThemeModule> {
      return {
        ngModule: ThemeModule,
        providers: [
          ...NbThemeModule.forRoot(
            {
              name: 'cosmic',
            },
            [ CORPORATE_THEME ],
          ).providers || [],
        ],
      };
    }
  }