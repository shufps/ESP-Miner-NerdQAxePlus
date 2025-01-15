import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';


interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
}

@Injectable({
  providedIn: 'root'
})
export class GithubUpdateService {

  constructor(
    private httpClient: HttpClient
  ) { }


  public getReleases(): Observable<GithubRelease[]> {
    return this.httpClient.get<GithubRelease[]>(
      'https://api.github.com/repos/shufps/ESP-Miner-NerdQAxePlus/releases'
    ).pipe(
      map((releases: GithubRelease[]) =>
        releases.filter((release: GithubRelease) =>
          // Exclude prereleases and releases with "-rc" in the tag name.
          !release.prerelease && !release.tag_name.includes('-rc')
        )
      )
    );
  }

}
