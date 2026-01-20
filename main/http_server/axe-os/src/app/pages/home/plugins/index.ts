import { registerValuePillsPlugin } from './value-pills.plugin';

let _registered = false;

export function registerHomeChartPlugins(): void {
  if (_registered) return;
  _registered = true;
  registerValuePillsPlugin();
}
