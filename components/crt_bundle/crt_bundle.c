#include "crt_bundle.h"

#include <stdbool.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

static const char *TAG = "crt_bundle";

// not in the public header of ESP-IDF 5.3, but exported
int esp_crt_verify_callback(void *buf, mbedtls_x509_crt *crt, int depth, uint32_t *flags);

// the bundle ESP-IDF embeds (gen_crt_bundle.py format):
// u16 count, then per root: u16 name_len, u16 key_len, subject (DER), SubjectPublicKeyInfo (DER)
extern const uint8_t bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t bundle_end[] asm("_binary_x509_crt_bundle_end");

#define BUNDLE_HEADER_LEN 2
#define CRT_HEADER_LEN 4

// true if a root in the bundle has the same subject and public key as crt
static bool is_bundle_root(const mbedtls_x509_crt *crt)
{
    if (bundle_end - bundle_start < BUNDLE_HEADER_LEN) {
        return false;
    }

    uint16_t num_certs = (bundle_start[0] << 8) | bundle_start[1];
    const uint8_t *p = bundle_start + BUNDLE_HEADER_LEN;

    for (int i = 0; i < num_certs; i++) {
        if (p + CRT_HEADER_LEN > bundle_end) {
            return false;
        }
        size_t name_len = (p[0] << 8) | p[1];
        size_t key_len = (p[2] << 8) | p[3];
        const uint8_t *name = p + CRT_HEADER_LEN;
        const uint8_t *key = name + name_len;
        if (key + key_len > bundle_end) {
            return false;
        }

        if (crt->subject_raw.len == name_len && !memcmp(crt->subject_raw.p, name, name_len) &&
            crt->pk_raw.len == key_len && !memcmp(crt->pk_raw.p, key, key_len)) {
            return true;
        }
        p = key + key_len;
    }
    return false;
}

static int verify_callback(void *buf, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    // Only the topmost certificate of the chain is "not trusted" when mbedTLS
    // found no parent. The signatures below it were already checked by mbedTLS,
    // so if it carries a trusted root's name and key, the chain ends at that root.
    if ((*flags & ~MBEDTLS_X509_BADCERT_BAD_MD) == MBEDTLS_X509_BADCERT_NOT_TRUSTED && is_bundle_root(crt)) {
        ESP_LOGI(TAG, "certificate is a cross-signed trusted root, validated");
        *flags = 0;
        return 0;
    }
    return esp_crt_verify_callback(buf, crt, depth, flags);
}

esp_err_t crt_bundle_attach(void *conf)
{
    esp_err_t ret = esp_crt_bundle_attach(conf);
    if (ret == ESP_OK && conf) {
        mbedtls_ssl_conf_verify((mbedtls_ssl_config *) conf, verify_callback, NULL);
    }
    return ret;
}
