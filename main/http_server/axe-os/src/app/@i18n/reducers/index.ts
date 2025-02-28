import * as fromLanguage from './language.reducer';
import * as fromRoot from '../../@reducers';
import {
  combineReducers,
  Action,
  createFeatureSelector,
  createSelector
} from '@ngrx/store';

export const i18nFeatureKey = 'i18n';

export interface I18nState {
  [fromLanguage.languageFeatureKey]: fromLanguage.State;
}

export interface State extends fromRoot.State {
  [i18nFeatureKey]: I18nState;
}

export function reducers(state: I18nState | undefined, action: Action) {
  return combineReducers({
    [fromLanguage.languageFeatureKey]: fromLanguage.reducer
  })(state, action);
}

export const selectI18nState = createFeatureSelector<State, I18nState>(
  i18nFeatureKey
);

export const selectLanguageState = createSelector(
  selectI18nState,
  (state: I18nState) => state.language
);

export const getCurrentLanguage = createSelector(
  selectLanguageState,
  fromLanguage.getLanguage
);
