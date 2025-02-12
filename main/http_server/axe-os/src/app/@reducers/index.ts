import * as fromRouter from '@ngrx/router-store';
import { InjectionToken } from '@angular/core';
import { ActionReducerMap, Action, MetaReducer } from '@ngrx/store';

export interface State {
  router: fromRouter.RouterReducerState<any>;
}

export const ROOT_REDUCERS = new InjectionToken<
  ActionReducerMap<State, Action>
>('Root reducers token', {
  factory: () => ({
    router: fromRouter.routerReducer
  })
});

export const metaReducers: MetaReducer<State>[] = [];
