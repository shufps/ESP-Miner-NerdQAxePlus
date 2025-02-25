import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'split'
})
export class SplitPipe implements PipeTransform {

  transform(value:string, separator:string):string[] {
    let splits = value.split(separator);
    if(splits.length > 1) {
      return splits;
    } else {
      return [value];
    }
  }

}
