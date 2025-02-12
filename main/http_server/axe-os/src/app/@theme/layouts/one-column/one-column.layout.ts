import { Component, OnDestroy, OnInit } from '@angular/core';
import { NbMediaBreakpointsService, NbMenuService, NbSidebarService, NbThemeService } from '@nebular/theme';
import { Subject, takeUntil } from 'rxjs';
@Component({
  selector: 'ngx-one-column-layout',
  styleUrls: ['./one-column.layout.scss'],
  template: `
    <nb-layout>
      <nb-layout-header fixed>
        <ngx-header></ngx-header>
      </nb-layout-header>

      <nb-sidebar class="menu-sidebar" tag="menu-sidebar" fixed responsive>
        <ng-content select="nb-menu"></ng-content>
      </nb-sidebar>

      <nb-layout-column>
        <ng-content select="router-outlet"></ng-content>
      </nb-layout-column>

      <nb-layout-footer fixed>
        <ngx-footer></ngx-footer>
      </nb-layout-footer>
    </nb-layout>
  `,
})
export class OneColumnLayoutComponent implements OnInit, OnDestroy{
  $destroy = new Subject<void>()

  constructor(  
    private menuService: NbMenuService,
    private sidebarService: NbSidebarService,
    private breakpointService: NbMediaBreakpointsService)
  {

  }
  ngOnInit(): void {

    this.menuService.onItemClick().pipe(takeUntil(this.$destroy)).subscribe(
      {
        next: ()=>{
          setTimeout(()=>{
            const { sm , lg} = this.breakpointService.getBreakpointsMap(); 
            const width = document.documentElement.clientWidth
            if ( width < sm){
              this.sidebarService.collapse('menu-sidebar')
            } else if (width < lg){
              this.sidebarService.compact('menu-sidebar')
            }
          },30)
        }
      }
    )
}

ngOnDestroy(): void {
    
}
}
