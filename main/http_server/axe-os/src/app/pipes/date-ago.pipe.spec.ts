import { DateAgoPipe } from './date-ago.pipe';
import { TranslateService } from '@ngx-translate/core';

describe('DateAgoPipe', () => {
  const translateServiceMock = {
    instant: (key: string) => key,
  } as Partial<TranslateService> as TranslateService;

  it('create an instance', () => {
    const pipe = new DateAgoPipe(translateServiceMock);
    expect(pipe).toBeTruthy();
  });
});
