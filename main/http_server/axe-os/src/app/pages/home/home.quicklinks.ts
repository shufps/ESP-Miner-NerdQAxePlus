/**
 * @docs Home Quicklinks Helper
 *
 * This module centralizes all pool-specific logic related to:
 *  - Building dashboard / statistics URLs ("quick links") for known mining pools
 *  - Normalizing stratum URLs (supports stratum+tcp://, host:port, host)
 *  - Determining pool capabilities (e.g. ICMP ping support)
 *
 * Motivation:
 * - Keep Angular components free from pool-specific string matching
 * - Provide a single, easily extensible registry for mining pool metadata
 * - Ensure robust behavior across different stratum URL formats
 *
 * Usage:
 * ```ts
 * import { getQuickLink, supportsPing } from './home.quicklinks';
 *
 * const url = getQuickLink(stratumURL, stratumUser);
 * const canPing = supportsPing(stratumURL);
 * ```
 *
 * Adding a new pool:
 * - Append a new entry to `QUICKLINK_RULES`
 * - Order matters: first matching rule wins
 */

type QuickLinkRule = {
  /** Returns true if the rule applies to the given pool host */
  match: (host: string, raw: string) => boolean;

  /** Builds the pool-specific dashboard URL */
  build: (address: string) => string;
};

/**
 * Ensures that a string can be parsed by the `URL` constructor.
 *
 * If no URL scheme is present, `http://` is prepended.
 * This allows parsing of:
 *  - stratum+tcp://host:port
 *  - host:port
 *  - host
 */
function toUrlLike(raw: string): string {
  if (/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(raw)) {
    return raw;
  }
  return `http://${raw}`;
}

/**
 * Extracts and normalizes the hostname from a stratum URL.
 *
 * The returned hostname is always lowercased and free of ports,
 * protocols or paths.
 *
 * @param stratumURL Raw stratum URL or host string
 * @returns Normalized hostname (best-effort)
 */
function extractHost(stratumURL: string): string {
  try {
    return new URL(toUrlLike(stratumURL)).hostname.toLowerCase();
  } catch {
    // Fallback for malformed input
    return (stratumURL ?? '').toLowerCase();
  }
}

/**
 * Pool-specific quick link rules.
 *
 * Each rule:
 * - Decides if it applies via `match`
 * - Generates a dashboard / stats URL via `build`
 *
 * IMPORTANT:
 * - Order matters
 * - The first matching rule wins
 */
const QUICKLINK_RULES: QuickLinkRule[] = [
  {
    match: (h) => h.includes('public-pool.io'),
    build: (a) => `https://web.public-pool.io/#/app/${a}`,
  },
  {
    match: (h) => h.includes('ocean.xyz'),
    build: (a) => `https://ocean.xyz/stats/${a}`,
  },
  {
    match: (h) => h.includes('solo.d-central.tech'),
    build: (a) => `https://solo.d-central.tech/#/app/${a}`,
  },

  // CKPool variants
  {
    match: (h) => /^eusolo[46]?\.(ckpool\.org)$/.test(h),
    build: (a) => `https://eusolostats.ckpool.org/users/${a}`,
  },
  {
    match: (h) => /^solo[46]?\.(ckpool\.org)$/.test(h),
    build: (a) => `https://solostats.ckpool.org/users/${a}`,
  },

  {
    match: (h) => h.includes('pool.noderunners.network'),
    build: (a) => `https://noderunners.network/en/pool/user/${a}`,
  },
  {
    match: (h) => h.includes('satoshiradio.nl'),
    build: (a) => `https://pool.satoshiradio.nl/user/${a}`,
  },
  {
    match: (h) => h.includes('solohash.co.uk'),
    build: (a) => `https://solohash.co.uk/user/${a}`,
  },
  {
    match: (h) => h.includes('parasite.wtf'),
    build: (a) => `https://parasite.space/user/${a}`,
  },
  {
    match: (h) => h.includes('solomining.de'),
    build: (a) => `https://pool.solomining.de/#/app/${a}`,
  },
  {
    match: (h) => h.includes('atlaspool.io'),
    build: (a) => `https://atlaspool.io/dashboard.html?wallet=${a}`,
  },
];

/**
 * Builds a pool-specific dashboard / statistics URL
 * for the given stratum endpoint and user.
 *
 * Behavior:
 * - Extracts the wallet/address from the stratum user (`wallet.worker`)
 * - Matches the pool against known quick link rules
 * - Returns a pool-specific dashboard URL if available
 * - Falls back to a normalized URL representation of the stratum endpoint
 *
 * @param stratumURL  Stratum pool URL or host
 * @param stratumUser Stratum user string (wallet[.worker])
 * @returns Pool dashboard URL or `undefined` if input is empty
 */
export function getQuickLink(
  stratumURL: string,
  stratumUser: string
): string | undefined {
  const safeUrl = stratumURL ?? '';
  const safeUser = stratumUser ?? '';

  // Avoid returning "http://" for completely empty input
  if (!safeUrl.trim() && !safeUser.trim()) {
    return undefined;
  }

  const address = safeUser.split('.', 1)[0] || '';
  const host = extractHost(safeUrl);

  const rule = QUICKLINK_RULES.find((r) => r.match(host, safeUrl));
  if (rule) {
    return rule.build(address);
  }

  // Default fallback: show the stratum endpoint as a URL
  return safeUrl.startsWith('http') ? safeUrl : toUrlLike(safeUrl);
}

/**
 * Indicates whether the given pool supports ICMP ping.
 *
 * Some pools intentionally block or ignore ping requests.
 * This helper centralizes such pool-specific exceptions.
 *
 * @param stratumURL Stratum pool URL or host
 * @returns `true` if ping is supported, otherwise `false`
 */
export function supportsPing(stratumURL: string): boolean {
  const host = extractHost(stratumURL ?? '');
  return !host.includes('public-pool.io');
}
