import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'humanReadable'
})
export class HumanReadablePipe implements PipeTransform {

  private static _this = new HumanReadablePipe();

  public static transform(value: number): string {
    return this._this.transform(value);
  }

  // Reusable SI formatter with K/M/G/T/P/E
  public transform(value: number, digits: number = 2): string {
    // Handle NaN / Infinity early
    if (!Number.isFinite(value)) {
      return '0';
    }

    const suffixes = ['', 'K', 'M', 'G', 'T', 'P', 'E']; // 10^0 ... 10^18
    const negative = value < 0;
    let abs = Math.abs(value);
    let idx = 0;

    // Iterate up through units in 1000 steps
    while (abs >= 1000 && idx < suffixes.length - 1) {
      abs /= 1000;
      idx++;
    }

    // Optional: trim trailing .00 / .10 etc.
    let str = abs.toFixed(digits);
    if (digits > 0) {
      // remove trailing zeros and possibly trailing dot
      str = str.replace(/(\.\d*?[1-9])0+$/, '$1').replace(/\.0+$/, '');
    }

    return (negative ? '-' : '') + str + suffixes[idx];
  }



}
