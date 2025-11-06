import { Pipe, PipeTransform } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

@Pipe({
  name: 'dateAgo',
  pure: true
})
export class DateAgoPipe implements PipeTransform {

  constructor(private translateService: TranslateService) {}

  transform(value: any, args?: any): any {
    if (value) {
      value = new Date().getTime() - value * 1000;
      const seconds = Math.floor((+new Date() - +new Date(value)) / 1000);
      if (seconds < 29) // less than 30 seconds ago will show as 'Just now'
        return this.translateService.instant('COMMON.JUST_NOW');
      const intervals: { [key: string]: number } = {
        'UNITS.YEAR': 31536000,
        'UNITS.MONTH': 2592000,
        'UNITS.WEEK': 604800,
        'UNITS.DAY': 86400,
        'UNITS.HOUR': 3600,
        'UNITS.MINUTE': 60,
        'UNITS.SECOND': 1
      };
      let counter;
      for (const i in intervals) {
        counter = Math.floor(seconds / intervals[i]);
        if (counter > 0)
          if (counter === 1) {
            return counter + ' ' + this.translateService.instant(i + '_SINGULAR'); // singular (1 day ago)
          } else {
            return counter + ' ' + this.translateService.instant(i + '_PLURAL'); // plural (2 days ago)
          }
      }
    }
    return value;
  }

}