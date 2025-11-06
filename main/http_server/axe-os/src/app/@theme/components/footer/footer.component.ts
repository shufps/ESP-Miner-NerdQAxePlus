import { Component } from '@angular/core';

@Component({
  selector: 'ngx-footer',
  styleUrls: ['./footer.component.scss'],
  template: `
    <span class="created-by" [innerHTML]="'FOOTER.POWERED_BY' | translate">
    </span>
  `,
})
export class FooterComponent {}
