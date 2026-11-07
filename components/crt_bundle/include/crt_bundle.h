#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Drop-in replacement for esp_crt_bundle_attach() (same embedded bundle).
//
// ESP-IDF's bundle verify callback only looks up the issuer of the topmost
// certificate a server sends. Servers that send a cross-signed root (e.g.
// "GTS Root R4" issued by "GlobalSign Root CA" for discord.com and everything
// on Google Trust Services) fail when that issuer is not in the bundle, even
// though the root itself is. This callback additionally accepts the topmost
// certificate if its subject and public key match a root in the bundle.
// No roots are added: it is the same trust anchor, only with another issuer.
esp_err_t crt_bundle_attach(void *conf);

#ifdef __cplusplus
}
#endif
