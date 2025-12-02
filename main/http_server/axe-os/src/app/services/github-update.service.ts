import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, EMPTY } from 'rxjs';
import { map, switchMap, expand, scan, takeWhile, last } from 'rxjs/operators';

interface GithubAsset {
  id: number;
  name: string;
  browser_download_url: string;
  size: number;
}

export interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
  body: string;
  published_at: string;
  assets: GithubAsset[];
  isLatest?: boolean;
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

  private readonly baseReleasesUrl =
    'https://api.github.com/repos/shufps/ESP-Miner-NerdQAxePlus/releases';

  constructor(
    private httpClient: HttpClient
  ) { }

  /** Fetch a single page of releases */
  private fetchReleasePage(page: number, perPage = 50): Observable<GithubRelease[]> {
    const url = `${this.baseReleasesUrl}?per_page=${perPage}&page=${page}`;
    return this.httpClient.get<GithubRelease[]>(url);
  }

  /**
   * Fetch releases of ONE type (stable or prerelease) until we have
   * at least `targetCount` items or there are no more pages.
   */
  private loadReleasesOfType(
    includePrereleases: boolean,
    targetCount = 10,
    maxPages = 10,
    perPage = 50
  ): Observable<GithubRelease[]> {
    const isStable = (r: GithubRelease) =>
      !r.prerelease && !r.tag_name.includes('-rc');

    const isPre = (r: GithubRelease) =>
      r.prerelease || r.tag_name.includes('-rc');

    const matchesType = includePrereleases ? isPre : isStable;

    // start with page 1
    return this.fetchReleasePage(1, perPage).pipe(
      expand((releases, index) => {
        const nextPage = index + 2; // index starts at 0 (page 1)
        const isLastPage = releases.length < perPage;
        const reachedMaxPages = nextPage > maxPages;

        if (isLastPage || reachedMaxPages) {
          return EMPTY;
        }

        return this.fetchReleasePage(nextPage, perPage);
      }),
      // accumulate only matching releases
      scan((acc, releases) => {
        const filtered = releases.filter(matchesType);
        return acc.concat(filtered);
      }, [] as GithubRelease[]),
      // solange weiter sammeln, bis wir genug haben
      takeWhile(acc => acc.length < targetCount, true),
      // am Ende letztes akkumuliertes Array liefern
      last(),
      // auf gewünschte Anzahl begrenzen
      map(acc => acc.slice(0, targetCount))
    );
  }

  /**
   * Fetch either:
   *  - up to 10 stable releases (includePrereleases = false)
   *  - up to 10 prereleases (includePrereleases = true)
   *
   * Es werden mehrere Seiten geladen, bis genug Releases vom gewünschten Typ
   * gefunden wurden oder keine Releases mehr da sind.
   */
  public getReleases(includePrereleases = false): Observable<GithubRelease[]> {
    const latest$ = this.httpClient.get<GithubRelease>(
      `${this.baseReleasesUrl}/latest`
    );

    const selected$ = this.loadReleasesOfType(includePrereleases, 10, 10, 50);

    return selected$.pipe(
      switchMap((releases: GithubRelease[]) =>
        latest$.pipe(
          map((latest) =>
            releases.map(r => ({
              ...r,
              body: r.body || '',
              isLatest: !includePrereleases && r.id === latest.id
            }))
          )
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
