import { HttpClient, HttpEventType } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import { map, catchError } from 'rxjs/operators';


interface GithubAsset {
  name: string;
  browser_download_url: string;
  size: number;
}

interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
  body: string;
  published_at: string;
  assets: GithubAsset[];
}

export interface VersionComparison {
  isNewer: boolean;
  isSame: boolean;
  isOlder: boolean;
  currentVersion: string;
  latestVersion: string;
}

export enum UpdateStatus {
  UP_TO_DATE = 'up-to-date',
  UPDATE_AVAILABLE = 'update-available',
  OUTDATED = 'outdated',
  UNKNOWN = 'unknown'
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

  /**
   * Compare two semantic versions
   * Returns: 1 if v1 > v2, -1 if v1 < v2, 0 if equal
   */
  public compareVersions(v1: string, v2: string): number {
    // Remove 'v' prefix if present
    const cleanV1 = v1.replace(/^v/, '');
    const cleanV2 = v2.replace(/^v/, '');

    const parts1 = cleanV1.split('.').map(p => parseInt(p) || 0);
    const parts2 = cleanV2.split('.').map(p => parseInt(p) || 0);

    for (let i = 0; i < Math.max(parts1.length, parts2.length); i++) {
      const p1 = parts1[i] || 0;
      const p2 = parts2[i] || 0;

      if (p1 > p2) return 1;
      if (p1 < p2) return -1;
    }

    return 0;
  }

  /**
   * Compare current version with latest release
   */
  public getVersionComparison(currentVersion: string, latestRelease: GithubRelease): VersionComparison {
    const comparison = this.compareVersions(currentVersion, latestRelease.tag_name);

    return {
      isNewer: comparison > 0,
      isSame: comparison === 0,
      isOlder: comparison < 0,
      currentVersion,
      latestVersion: latestRelease.tag_name
    };
  }

  /**
   * Get update status based on version comparison
   */
  public getUpdateStatus(currentVersion: string, latestRelease: GithubRelease | null): UpdateStatus {
    if (!latestRelease) {
      return UpdateStatus.UNKNOWN;
    }

    const comparison = this.compareVersions(currentVersion, latestRelease.tag_name);

    if (comparison === 0) {
      return UpdateStatus.UP_TO_DATE;
    } else if (comparison < 0) {
      return UpdateStatus.UPDATE_AVAILABLE;
    } else {
      return UpdateStatus.OUTDATED;
    }
  }

  /**
   * Download firmware directly from GitHub
   */
  public downloadFirmware(url: string): Observable<any> {
    return this.httpClient.get(url, {
      responseType: 'blob',
      reportProgress: true,
      observe: 'events'
    });
  }

  /**
   * Get changelog formatted as HTML
   */
  public getChangelog(release: GithubRelease): string {
    if (!release.body) {
      return 'No changelog available';
    }

    // Convert markdown to basic HTML (simple conversion)
    return release.body
      .replace(/### (.*)/g, '<h4>$1</h4>')
      .replace(/## (.*)/g, '<h3>$1</h3>')
      .replace(/# (.*)/g, '<h2>$1</h2>')
      .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
      .replace(/\*(.*?)\*/g, '<em>$1</em>')
      .replace(/\n/g, '<br>');
  }

  /**
   * Find asset in release by filename
   */
  public findAsset(release: GithubRelease, filename: string): GithubAsset | undefined {
    return release.assets.find(asset => asset.name === filename);
  }

}
