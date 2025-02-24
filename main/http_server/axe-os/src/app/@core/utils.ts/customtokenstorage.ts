import { Injectable } from "@angular/core";
import { NbAuthToken, NbAuthTokenParceler, NbTokenStorage } from "@nebular/auth";
import { StorageService } from "./storage.service";

@Injectable()
export class NbCustomTokenStorage extends NbTokenStorage {

  protected key = 'your_secret_key';

  constructor(private parceler: NbAuthTokenParceler, private storageService : StorageService) {
    super();
  }

  /**
   * Returns token from localStorage
   * @returns {NbAuthToken}
   */
  get(): NbAuthToken {
    //const raw = localStorage.getItem(this.key);
    const raw = this.storageService.secureStorage.getItem(this.key);
    return this.parceler.unwrap(raw);
  }

  /**
   * Sets token to localStorage
   * @param {NbAuthToken} token
   */
  set(token: NbAuthToken) {
    const encoded = btoa(JSON.stringify(token));
    const raw = this.parceler.wrap(token);
    //localStorage.setItem(this.key, raw);
    this.storageService.secureStorage.setItem(this.key, raw);
  }

  /**
   * Clears token from localStorage
   */
  clear() {
    //localStorage.removeItem(this.key);
    return this.storageService.secureStorage.removeItem(this.key);

  }
}
