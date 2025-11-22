import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'humanReadable'
})
export class HumanReadablePipe implements PipeTransform {

  private static _this = new HumanReadablePipe();

  public static transform(value: number): string {
    return this._this.transform(value);
  }

  public transform(value: number): string {
    return Intl.NumberFormat('en', { notation: 'compact' }).format(value)
  }



}
