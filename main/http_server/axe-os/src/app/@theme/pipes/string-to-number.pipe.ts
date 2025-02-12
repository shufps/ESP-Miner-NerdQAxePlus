import { Pipe, PipeTransform } from '@angular/core';

@Pipe({ name: 'stringToNumber'}) export class StringToNumberPipe implements PipeTransform {

transform(input: string): number{ //string type
   return parseInt(input, 10);
} }