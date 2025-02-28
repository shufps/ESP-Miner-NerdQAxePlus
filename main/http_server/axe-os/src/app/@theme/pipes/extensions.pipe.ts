import { Pipe, PipeTransform } from '@angular/core';

@Pipe({ name: 'extension' })

export class RemoveExtension implements PipeTransform {
  transform(item) {
    return item.substring(0, item.indexOf('.'))
  }

}