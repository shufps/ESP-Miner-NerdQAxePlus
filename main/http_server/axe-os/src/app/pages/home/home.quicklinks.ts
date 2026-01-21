/**
 * @docs Home Quicklinks Helper
 *
 * Central place for pool-related mapping used by the Home page.
 *
 * Responsibilities:
 * - Normalize stratum URLs and extract hostnames
 * - Generate pool-specific dashboard/statistics URLs (quick links)
 * - Detect pool capabilities (e.g. ICMP ping support)
 * - Resolve pool icons with the following priority:
 *
 *   1) Manual icon override (PoolMeta.iconUrl)
 *   2) Custom favicon path (PoolMeta.faviconPath + faviconHost)
 *   3) Default favicon fallback: https://<host>/favicon.ico
 *
 * Additionally:
 * - Local pools (private IP/localhost/.local) are handled via registry rule
 *   and return DEFAULT_POOL_ICON_URL to avoid broken favicons.
 *
 * Notes:
 * - No external favicon services are used
 * - No network existence checks are performed
 * - Order matters: first matching pool wins
 */

export type PoolCapabilities = {
  /** ICMP ping is supported/sensible */
  ping?: boolean;
};

export const DEFAULT_POOL_ICON_URL = 'assets/pools/default.svg';
export const DEFAULT_EXTERNAL_POOL_ICON_URL = 'assets/pools/public.svg';

export type PoolMeta = {
  id: string;
  name: string;

  /** Match against normalized hostname (lowercased, no port) */
  match: (host: string, raw: string) => boolean;

  /** Optional pool dashboard / stats URL builder */
  quickLink?: (address: string) => string;

  /** Optional pool capabilities */
  caps?: PoolCapabilities;

  /**
   * Optional hostname override used for favicon resolution.
   * Example: pool UI on parasite.space, favicon on parasite.wtf
   */
  faviconHost?: string;

  /**
   * Optional custom favicon path.
   * Allows svg/png or nested paths.
   *
   * Examples:
   *  - '/favicon.svg'
   *  - '/assets/favicon.png'
   *  - '/img/logo.svg'
   */
  faviconPath?: string;

  /**
   * Optional manual icon override.
   * Can be a local asset path or absolute URL.
   * Takes highest priority.
   */
  iconUrl?: string;
};

/* ------------------------------------------------------------------
 * URL / host helpers
 * ------------------------------------------------------------------ */

/**
 * Ensures the input can be parsed by the `URL` constructor.
 * If no scheme is present, `http://` is prepended.
 */
function toUrlLike(raw: string): string {
  if (/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(raw)) {
    return raw;
  }
  return `http://${raw}`;
}

/**
 * Extracts a normalized hostname from a stratum URL.
 *
 * Supports:
 * - stratum+tcp://host:port
 * - host:port
 * - host
 */
function extractHost(stratumURL: string): string {
  try {
    return new URL(toUrlLike(stratumURL)).hostname.toLowerCase();
  } catch {
    return (stratumURL ?? '').toLowerCase();
  }
}

/**
 * Local host heuristics for pools running on the local network.
 * For these endpoints, a favicon usually does not exist (no web server),
 * and HTTPS is often not available. We use a local default icon instead.
 */
function isIp(host: string): boolean {
  return /^\d{1,3}(\.\d{1,3}){3}$/.test(host);
}

function isPrivateIp(host: string): boolean {
  if (!isIp(host)) return false;
  const [a, b] = host.split('.').map((n) => parseInt(n, 10));
  if (a === 10) return true;
  if (a === 192 && b === 168) return true;
  if (a === 172 && b >= 16 && b <= 31) return true;
  return false;
}

/**
 * Detects localhost / private network endpoints.
 *
 * Includes:
 * - localhost
 * - 127.0.0.1
 * - ::1 (IPv6 localhost)
 * - *.local
 * - private IPv4 ranges
 */
export function isLocalHost(host: string): boolean {
  const h = (host ?? '').toLowerCase();

  // IPv6 localhost
  if (h === '::1') return true;

  // classic localhost
  if (h === 'localhost') return true;

  // mDNS / local domains
  if (h.endsWith('.local')) return true;

  // IPv4 localhost
  if (h === '127.0.0.1') return true;

  // private IPv4 ranges
  if (isPrivateIp(h)) return true;

  return false;
}

/* ------------------------------------------------------------------
 * Pool registry (first match wins)
 * ------------------------------------------------------------------ */

const POOLS: PoolMeta[] = [
  /**
   * Local pool rule (must be FIRST).
   * Ensures local endpoints do not try to load https://<ip>/favicon.ico
   * and instead use the default icon.
   *
   * Example -> Parasite pool:
   * - UI: parasite.space
   * - Icon may be hosted elsewhere or have custom path
   *
   * If you ever need a custom path or different domain:
   * - set faviconHost + faviconPath
   *
   * Example:
   *   faviconHost: 'parasite.wtf',
   *   faviconPath: '/favicon.svg',
   */
  {
    // Prefer Umbrel over other local pools.
    id: 'umbrel-local',
    name: 'Umbrel',
    match: (h) => h === 'umbrel.local',
    iconUrl: 'assets/pools/umbrel.svg',
  },
  {
    id: 'local-pool',
    name: 'Local pool',
    match: (h) => isLocalHost(h),
    iconUrl: DEFAULT_POOL_ICON_URL,
  },

  {
    id: 'public-pool',
    name: 'public-pool.io',
    match: (h) => h.includes('public-pool.io'),
    quickLink: (a) => `https://web.public-pool.io/#/app/${a}`,
    iconUrl: 'assets/pools/public-pool.png',
    caps: { ping: false },
  },
  {
    id: 'ocean',
    name: 'ocean.xyz',
    match: (h) => h.includes('ocean.xyz'),
    quickLink: (a) => `https://ocean.xyz/stats/${a}`,
  },
  // CKPool variants
  {
    id: 'ckpool-eusolo',
    name: 'eusolo*.ckpool.org',
    match: (h) => /^eusolo[46]?\.(ckpool\.org)$/.test(h),
    quickLink: (a) => `https://eusolostats.ckpool.org/users/${a}`,
    iconUrl: '/assets/pools/ck-eupool.svg',
  },
  {
    id: 'ckpool-solo',
    name: 'solo*.ckpool.org',
    match: (h) => /^solo[46]?\.(ckpool\.org)$/.test(h),
    quickLink: (a) => `https://solostats.ckpool.org/users/${a}`,
    iconUrl: '/assets/pools/ck-pool.svg',
  },
  {
    id: 'ckpool-ausolo',
    name: 'ausolo*.ckpool.org',
    match: (h) => /^ausolo[46]?\.(ckpool\.org)$/.test(h),
    quickLink: (a) => `https://ausolostats.ckpool.org/users/${a}`,
    iconUrl: '/assets/pools/ck-aupool.svg',
  },
  {
    id: 'noderunners',
    name: 'pool.noderunners.network',
    match: (h) => h.includes('pool.noderunners.network'),
    quickLink: (a) => `https://noderunners.network/en/pool/user/${a}`,
    faviconHost: 'noderunners.network',
  },
  {
    id: 'satoshiradio',
    name: 'satoshiradio.nl',
    match: (h) => h.includes('satoshiradio.nl'),
    quickLink: (a) => `https://pool.satoshiradio.nl/user/${a}`,
    faviconHost: 'satoshiradio.nl',
    faviconPath: '/assets/SR_Logo_Orange.webp',
  },
  {
    id: 'solohash',
    name: 'solohash.co.uk',
    match: (h) => h.includes('solohash.co.uk'),
    quickLink: (a) => `https://solohash.co.uk/user/${a}`,
    faviconHost: 'solohash.co.uk',
    faviconPath: '/icons/favicon.ico',
  },
  {
    id: 'parasite',
    name: 'parasite',
    match: (h) => h.includes('parasite.space') || h.includes('parasite.wtf'),
    quickLink: (a) => `https://parasite.space/user/${a}`,
    faviconHost: 'parasite.space',
    faviconPath: '/favicon.ico',
  },
  {
    id: 'solomining-de',
    name: 'solomining.de',
    match: (h) => h.includes('solomining.de'),
    quickLink: (a) => `https://pool.solomining.de/#/app/${a}`,
  },
  {
    id: 'atlaspool',
    name: 'atlaspool.io',
    match: (h) => h.includes('atlaspool.io'),
    quickLink: (a) => `https://atlaspool.io/dashboard.html?wallet=${a}`,
    faviconHost: 'atlaspool.io',
    faviconPath: '/favicon.ico',
  },
  {
    id: 'powermining',
    name: 'powermining.io',
    match: (h) => h.includes('powermining.io'),
    quickLink: (a) => `https://pool.powermining.io/#/app/${a}`,
  },
  {                                                                                                                                                                                                 
      id: 'blitzpool',                                                                                                                                                                              
      name: 'blitzpool.yourdevice.ch',                                                                                                                                                              
      match: (h) => h.includes('blitzpool.yourdevice.ch'),                                                                                                                                          
      quickLink: (a) => `https://blitzpool.yourdevice.ch/#/app/${a}`,                                                                                                                               
  },     
];

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/**
 * Detects pool metadata for a given stratum endpoint.
 *
 * @param stratumURL Stratum pool URL or host
 */
export function detectPoolMeta(stratumURL: string): PoolMeta | undefined {
  const raw = stratumURL ?? '';
  const host = extractHost(raw);
  return POOLS.find((p) => p.match(host, raw));
}

/**
 * Builds a pool-specific dashboard / statistics URL.
 *
 * @param stratumURL  Stratum pool URL or host
 * @param stratumUser Stratum user string (wallet[.worker])
 */
export function getQuickLink(
  stratumURL: string,
  stratumUser: string
): string | undefined {
  const safeUrl = stratumURL ?? '';
  const safeUser = stratumUser ?? '';

  if (!safeUrl.trim() && !safeUser.trim()) {
    return undefined;
  }

  const address = safeUser.split('.', 1)[0] || '';
  const pool = detectPoolMeta(safeUrl);

  if (pool?.quickLink) {
    return pool.quickLink(address);
  }

  return safeUrl.startsWith('http') ? safeUrl : toUrlLike(safeUrl);
}

/**
 * Indicates whether the given pool supports ICMP ping.
 *
 * @param stratumURL Stratum pool URL or host
 */
export function supportsPing(stratumURL: string): boolean {
  const pool = detectPoolMeta(stratumURL ?? '');
  return pool?.caps?.ping ?? true;
}

/**
 * Resolves a pool icon URL.
 *
 * Priority:
 * 1) Manual icon override (PoolMeta.iconUrl)
 * 2) Custom favicon path (PoolMeta.faviconPath + faviconHost)
 * 3) Default favicon.ico at https://<host>/favicon.ico
 *
 * Local pools are handled by a registry rule which sets iconUrl to DEFAULT_POOL_ICON_URL.
 *
 * @param stratumURL Stratum pool URL or host
 * @returns Icon URL (may be DEFAULT_POOL_ICON_URL)
 */
export function getPoolIconUrl(stratumURL: string): string {
  const safeUrl = stratumURL ?? '';
  const pool = detectPoolMeta(safeUrl);

  // 1) Manual icon override (including local-pool rule)
  if (pool?.iconUrl?.trim()) {
    return pool.iconUrl.trim();
  }

  const host = extractHost(safeUrl);
  const faviconHost = (pool?.faviconHost ?? host).trim();

  // If we cannot derive a host, fall back to a local default icon
  if (!faviconHost) {
    return DEFAULT_POOL_ICON_URL;
  }

  // 2) Custom favicon path (svg/png/etc.)
  if (pool?.faviconPath?.trim()) {
    return `https://${faviconHost}${pool.faviconPath.trim()}`;
  }

  // 3) Default favicon.ico
  return `https://${faviconHost}/favicon.ico`;
}
