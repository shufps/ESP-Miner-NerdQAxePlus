import { Language } from '../models/language.model';
import { createReducer, on } from '@ngrx/store';
import { LanguageActions } from '../actions';

export const languageFeatureKey = 'language';

export interface State {
  current: Language;
}

export const initialState: State = {
  current: (navigator.language.toLowerCase().startsWith('fr')
    ? 'fr'
    : 'en') as Language
};

export const reducer = createReducer(
  initialState,
  on(LanguageActions.set, (state, { language }) => ({
    ...state,
    current: language
  }))
);

export const getLanguage = (state: State) => state.current;
