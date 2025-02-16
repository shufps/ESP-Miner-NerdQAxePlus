import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'ANSI',
  pure: true
})
export class ANSIPipe implements PipeTransform {
/*
  transform(value: string): string {
    return value.slice(9, value.length - 5);
  }
*/
  transform(value: string): string {
    if (!value) return '';

    const ansiHtml = value
            .replace(/\x1B\[\d*;?31m/g, '<span class="ansi-red">')  // Red
            .replace(/\x1B\[\d*;?32m/g, '<span class="ansi-green">') // Green
            .replace(/\x1B\[\d*;?33m/g, '<span class="ansi-yellow">') // Yellow
            .replace(/\x1B\[\d*;?34m/g, '<span class="ansi-blue">') // Blue
            .replace(/\x1B\[\d*;?35m/g, '<span class="ansi-magenta">') // Magenta
            .replace(/\x1B\[\d*;?36m/g, '<span class="ansi-cyan">') // Cyan
            .replace(/\x1B\[\d*;?37m/g, '<span class="ansi-white">') // White
            .replace(/\x1B\[0m/g, '</span>') // Reset

            // Ensure any unclosed spans get closed (fallback safety)
            .replace(/(\<span.*?\>)$/, '$1</span>');

    return ansiHtml;
  }

}
