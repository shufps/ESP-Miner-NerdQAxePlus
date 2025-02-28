import { Pipe, PipeTransform } from '@angular/core';

/**
 * Cast super type into type using generics
 * Return Type obtained by optional @param type OR assignment type.
 */

@Pipe({ name: 'cast' , pure: true,
})
export class CastPipe implements PipeTransform {

  transform<T>(value: any, _type: (new (...args: any[]) => T) | T): T {
    return value as T;
  }

}