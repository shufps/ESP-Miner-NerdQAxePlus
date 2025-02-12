import { Injectable } from "@angular/core";
import { Location } from "@angular/common";
import { Router, NavigationEnd } from "@angular/router";

@Injectable({ providedIn: "root" })
export class NavigationService {

  private history: string[] = [];

  constructor(private router: Router, private location: Location) {
    this.router.events.subscribe((event) => {
      if (event instanceof NavigationEnd) {
        this.history.push(event.urlAfterRedirects);
      }
    });
  }

  back(option?: string): void {
    if(option && option.length >0 ){
      this.router.navigate(option.split(","))
    }
    this.history.pop();
    if (this.history.length > 0) {
      console.log(this.history.at(this.history.length-1))
      this.router.navigateByUrl(this.history.at(this.history.length-1))
    } else {
      this.router.navigateByUrl("/");
    }
  }
}
