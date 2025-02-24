import { Component } from '@angular/core';

@Component({
  selector: 'ngx-footer',
  styleUrls: ['./footer.component.scss'],
  template: `
    <span class="created-by">
      Powered by <a href="https://github.com/akveo/ngx-admin" target="_blank">ngx-admin</a> and <a href="https://akveo.github.io/nebular" target="_blank"> Nebular.</a>
    </span>
  `,
})
export class FooterComponent {}
