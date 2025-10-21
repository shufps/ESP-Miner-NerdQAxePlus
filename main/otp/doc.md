# OTP Module – Technical Specification

Version: 1.0
Date: 2025-10-19

## Overview

The OTP module provides TOTP-based two-factor authentication (2FA) and signed session tokens for the Web API.

Key features:
- TOTP (RFC 4226/6238): 6 digits, 30-second steps, HMAC-SHA1.
- Replay protection: ±1 window with a 3-bit mask.
- Session tokens: compact, HMAC-SHA256-signed tokens.
- Key separation: distinct TOTP secret vs. session-signing key.
- Rate limiting for invalid OTP attempts.

This document covers data formats, flows, HTTP API, persistence, and security considerations.

---

## Components & Files

- otp.cpp/.h: TOTP, Base32, HMAC, session token mint/verify, QR generation, settings.
- http_utils.*: HTTP helpers, JSON, rate limiting, header parsing, OTP/session validation.
- handler_otp.*: HTTP endpoints for enrollment, status, session token issuance.
- qrcodegen.*: QR code generation for the otpauth URI.
- nvs_config.* / Config::*: Persistence (NVS).

---

## Cryptography & Formats

### Base32

- Alphabet: RFC 4648 uppercase, no padding "=".
- Encoder: emits the minimal number of characters.
- Decoder: tolerates non-alphabet characters by skipping them (useful for external inputs).
  When loading from NVS, the decoded length is strictly validated.

Lengths:
- Bytes → Base32: ceil(bytes * 8 / 5)
- 12 bytes → 20 chars
- 32 bytes → 52 chars

### HMAC

- TOTP/HOTP: HMAC-SHA1 (Google Authenticator/Authy defaults).
- Session tokens: HMAC-SHA256.

### TOTP Parameters

- Digits: 6
- Period: 30 s
- Step: step = now() / 30

### Replay Protection

- Accepts step-1, step, step+1.
- 3-bit mask (indices 0..2) prevents reusing an already accepted code within the ±1 window.
- Base step and mask are persisted after a successful validation.

---

## Session Tokens

### Goal

Reduce repeated TOTP prompts by issuing signed session tokens validated server-side.
Key separation: session key is not the TOTP secret.

### Structure

```
token = base32(payload) + "." + base32(hmac)

payload (12 bytes, packed, platform endianness):
    uint32_t iat;  // issued-at (UNIX seconds)
    uint32_t exp;  // expiry   (UNIX seconds)
    uint32_t bid;  // boot-id  (see below)

hmac (32 bytes):
    HMAC_SHA256(key = m_sessKey, msg = base32(payload))
```

Base32 lengths:
- base32(payload) = 20 chars
- base32(hmac)    = 52 chars
- Total           = 73 chars including the dot

### Boot ID (bid)

- Random 32-bit value.
- In the current design, bid is persisted (NVS) and remains stable across reboots, but is rotated when a new enrollment is initiated (new session key + new bid).
- Effect: Reboots do not invalidate sessions; starting a new enrollment invalidates all prior sessions.

---

## Flows

### Enrollment (initial setup / secret rotation)

1. Session setup: createSessionKey() generates
   - new m_sessKey (32 B random, Base32-persisted)
   - new m_bootId (random, persisted)
2. TOTP secret: 20 B random → Base32 (compact).
3. otpauth URI: otpauth://totp/<issuer>:<label>?secret=<B32>&issuer=<issuer>
4. QR code: URI encoded into a QR (version ≤ 10, ECC Medium) and shown on the device UI.
5. Enable: After successful TOTP verification, enabled = true is stored alongside the secret.

Consequence: Starting a new enrollment implicitly rotates m_sessKey and m_bootId → all existing sessions become invalid.

### Login / Access Control

- Prefer session token in header X-OTP-Session.
- Fallback: TOTP in header X-TOTP.

Validation order when OTP is enabled:
1. If force == false:
   - If X-OTP-Session present → verify session token (HMAC/exp/bid).
   - Otherwise (or if invalid) → verify X-TOTP (±1 window, replay mask).
2. If force == true (critical operations):
   - Only TOTP is accepted; session tokens are ignored.

On success, replay state may be updated and persisted.

---

## HTTP Headers & Endpoints

### Headers

- X-TOTP: 6-digit code (spaces tolerated).
- X-OTP-Session: session token (73 chars, see format).

### Endpoints (names reflect handler functions)

- POST_create_otp
  Purpose: start enrollment, generate QR, display on UI.
  Checks: network access allowed; OTP must not already be enabled.
  Response: 200 on success; 405/500 on error.

- PATCH_update_otp
  Purpose: finish enrollment, set enabled.
  Checks: network access allowed; TOTP required (force = true).
  JSON body: { "enabled": true | false }
  Effect: enrollment disabled, QR hidden, flag stored, secret persisted.

- GET_otp_status
  Purpose: read status.
  Response (JSON): { "enabled": <bool> }

- POST_create_otp_session
  Purpose: mint session token (default TTL: 24h).
  Checks: network access allowed; OTP enabled; TOTP required.
  Response (JSON):
  ```
  {
    "token": "<X-OTP-Session>",
    "ttlMs": 86400000,
    "expiresAt": <ms since epoch>
  }
  ```

(Note: Actual URL paths depend on the HTTP router; names here map to C handlers.)

---

## Persistence (NVS / Config)

Stored values (symbolic names via Config::*):
- OTPEnabled (bool)
- OTPSecret (Base32)
- OTPSessionKey (Base32 of 32 B; expects exactly 52 chars → 32 bytes after decode)
- OTTBootId (uint32)
- OTPReplayState
  - base_step (int64)
  - used_mask (uint8, only bits 0..2 used)

Load-time validation:
- OTPSessionKey: Base32 decode must yield exactly 32 bytes; otherwise discard and mark key absent.
- OTPSecret: Decoded as needed; on failure OTP is unusable until re-enrolled.

---

## Rate Limiting (invalid login attempts)

Parameters:
- RL_FAIL_LIMIT = 5 attempts
- RL_WINDOW_SEC = 60 seconds window
- RL_BLOCK_SEC = 300 seconds block time

Mechanics:
- Maintain most recent failure timestamps (millisecond resolution).
- If 5 failures occur within 60 s: block further attempts for 5 minutes (subsequent failures do not extend the block).
- On expiry, block state is cleared and history reset.

Notes:
- Block state is not persisted (process-local only).
- Minimal error information is returned while blocked.

---

## Error Codes & Responses

- 401 Unauthorized:
  - Missing/invalid OTP or session token.
  - Active block due to rate limiting.
- 405 Method Not Allowed:
  - Enrollment started while OTP already enabled.
  - Session mint while OTP disabled.
- 400 Bad Request:
  - Invalid JSON or missing fields.
- 500 Internal Server Error:
  - Resource/NVS errors, QR/serialization failures, invalid persisted data.

Response bodies:
- JSON for status/session endpoints.
- Short text message for errors elsewhere.

---

## Security Considerations & Decisions

1. Key separation
   TOTP secret ≠ session key. Enables targeted rotation and limits blast radius.

2. Token binding via bid
   bid is enrollment-epoch-bound (stable across reboot, rotated on enrollment).
   Result: reboots do not invalidate sessions; enrollment rotation does.

3. Constant-time HMAC compare
   Session HMAC verification uses constant-time comparison (mbedtls_ssl_safer_memcmp) to avoid timing oracles.

4. TOTP replay protection
   ±1 window with a 3-bit mask prevents reusing an accepted code within the window.

5. Strict Base32 validation for NVS
   Session key from NVS must decode to exactly 32 bytes; otherwise it is rejected.

6. Rate limiting
   Server-side throttling against brute force; ms-accurate timing.

7. Auth order for critical paths
   In PATCH_update_otp, UI/state changes occur only after successful TOTP validation.

---

## Compact Reference (Pseudocode)

### TOTP Validation (simplified)

```
parse user_code (6 digits; ignore spaces)
decode key = Base32(secret)

(base_step, used_mask) = load_replay_state()
step = now()/30

if base_step == 0 or step > base_step+1:
    base_step = step
    used_mask = 0
else if step < base_step-1:
    return INVALID  // too old

for off in {-1,0,+1}:
    s = step + off
    idx = s - (base_step - 1)   // 0..2
    if idx not in [0..2]: continue
    if used_mask & (1<<idx): continue
    if hotp_sha1(key, s) == user_code:
        used_mask |= (1<<idx)
        save_replay_state(base_step, used_mask & 0x07)
        return OK

return INVALID
```

### Session Token Verification (simplified)

```
split token at '.'
payload_b32 = left
sig_b32     = right

p    = base32_decode(payload_b32)  // 12 bytes
sref = base32_decode(sig_b32)      // 32 bytes

mac = HMAC_SHA256(m_sessKey, payload_b32)
if !ct_equal(mac, sref): return INVALID

pl = parse(payload)
ts = now()

if pl.exp <= ts: return INVALID
if pl.iat > ts + 300: return INVALID  // 5 min max skew
if pl.bid != m_bootId: return INVALID

return OK
```

---

## Interoperability

- Authenticator apps: Compatible with Google Authenticator, Authy, 1Password, etc. (SHA1/6/30 defaults; standard otpauth URI).
- QR codes: Version ≤ 10, ECC Medium (readable on small 170 px displays).

---

## Maintenance & Rotation

- Session key rotation: Starting a new enrollment generates a new m_sessKey → all sessions invalidated. (An admin rotation endpoint could be added if needed.)
- TOTP secret rotation: Re-run enrollment to provision a new secret (users must rescan QR in their app).
- Time source: TOTP requires a correct system time (is_time_synced()).

---

## Limitations & Known Points

- Payload endianness: SessionPayload uses platform endianness (internal use). External parsers should rely on the Base32 string covered by HMAC, not the raw binary layout.
- Base32 decoder tolerance: Non-alphabet chars are skipped. For NVS loads, length is strictly checked.
- Rate limiting: Process-local; no global/persistent counters.

---

## Example: otpauth URI

```
otpauth://totp/<issuer>:<label>?secret=<BASE32>&issuer=<issuer>
```

- issuer and label are URL-encoded.
- Defaults (SHA1/6/30s) are omitted to keep the URI compact.

---

## Change Log (excerpt)

- 2025-10-19:
  - Constant-time HMAC compare for session tokens.
  - Rate limiting fixed to use millisecond timestamps consistently.
  - Auth order hardened in PATCH_update_otp.
  - Strict NVS load-time verification for OTPSessionKey (52 Base32 chars → 32 bytes).
  - This document created/updated.
