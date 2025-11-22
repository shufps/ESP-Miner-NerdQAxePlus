import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'hashSuffix'
})
export class HashSuffixPipe implements PipeTransform {

  private static _this = new HashSuffixPipe();

  public static transform(value: number): string {
    return this._this.transform(value);
  }

  transform(value: number): string {
    return " "+Intl.NumberFormat('en', { notation: 'compact' }).format(value)+"H/s";
  }
}
