import { createAction, props } from '@ngrx/store';

import { Language } from '../models/language.model';

export const set = createAction(
  '[Language] Set',
  props<{ language: Language }>()
);
