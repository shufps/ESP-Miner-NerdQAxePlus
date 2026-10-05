import { NbJSThemeOptions, COSMIC_THEME as baseTheme } from '@nebular/theme';

const baseThemeVariables = baseTheme.variables;

// JS-theme del tema "gaia": valores que consume el JavaScript de las
// gráficas (chart.js / echarts) cuando este tema está activo. Hereda de
// 'cosmic'. Si quieres cambiar el color de las gráficas en este tema, añade
// overrides dentro de `variables` (mira theme.cosmic.ts como referencia).
export const GAIA_THEME = {
  name: 'gaia',
  base: 'cosmic',
  variables: {
  },
} as NbJSThemeOptions;
