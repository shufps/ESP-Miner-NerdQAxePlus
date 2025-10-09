import { Language } from '../models/language.model';
import { createReducer, on } from '@ngrx/store';
import { LanguageActions } from '../actions';

export const languageFeatureKey = 'language';

export interface State {
  current: Language;
}

// Initialize language from localStorage first, fallback to browser language
const getInitialLanguage = (): Language => {
  const savedLang = localStorage.getItem('language');
  if (savedLang) {
    return savedLang as Language;
  }
  return (navigator.language.toLowerCase().startsWith('fr') ? 'fr' : 'en') as Language;
};

export const initialState: State = {
  current: getInitialLanguage()
};

export const reducer = createReducer(
  initialState,
  on(LanguageActions.set, (state, { language }) => ({
    ...state,
    current: language
  }))
);

export const getLanguage = (state: State) => state.current;
