#pragma once
#include "stratum_config.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#include "nvs_config.h"
#include "psram_allocator.h"

static const char *TAG = "StratumConfig";

static bool strEq(const char *a, const char *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return strcmp(a, b) == 0;
}

StratumConfig::StratumConfig(int pool)
{
    if (!pool) {
        m_primary = true;
        m_host = Config::getStratumURL();
        m_port = Config::getStratumPortNumber();
        m_user = Config::getStratumUser();
        m_password = Config::getStratumPass();
        m_enonceSub = Config::isStratumEnonceSubscribe();
        m_tls = Config::isStratumTLS();
    } else {
        m_primary = false;
        m_host = Config::getStratumFallbackURL();
        m_port = Config::getStratumFallbackPortNumber();
        m_user = Config::getStratumFallbackUser();
        m_password = Config::getStratumFallbackPass();
        m_enonceSub = Config::isStratumFallbackEnonceSubscribe();
        m_tls = Config::isStratumFallbackTLS();
    }
}

bool StratumConfig::reload()
{
    // Load new values
    char *newHost = m_primary ? Config::getStratumURL() : Config::getStratumFallbackURL();
    int newPort   = m_primary ? Config::getStratumPortNumber() : Config::getStratumFallbackPortNumber();
    char *newUser = m_primary ? Config::getStratumUser() : Config::getStratumFallbackUser();
    char *newPass = m_primary ? Config::getStratumPass() : Config::getStratumFallbackPass();
    bool newEnsub = m_primary ? Config::isStratumEnonceSubscribe() : Config::isStratumFallbackEnonceSubscribe();
    bool newTLS   = m_primary ? Config::isStratumTLS() : Config::isStratumFallbackTLS();
    // Compare
    bool same =
        strEq(m_host, newHost) &&
        m_port == newPort &&
        strEq(m_user, newUser) &&
        strEq(m_password, newPass) &&
        m_enonceSub == newEnsub &&
        m_tls == newTLS;

    if (same) {
        // Free temporary values (they were newly allocated by Config::get)
        safe_free(newHost);
        safe_free(newUser);
        safe_free(newPass);
        return false;
    }

    // Update fields: first free old values
    safe_free(m_host);
    safe_free(m_user);
    safe_free(m_password);

    m_host       = newHost;
    m_port       = newPort;
    m_user       = newUser;
    m_password   = newPass;
    m_enonceSub  = newEnsub;
    m_tls        = newTLS;

    return true;
}

void StratumConfig::copyInto(StratumConfig *dst)
{
    safe_free(dst->m_host);
    safe_free(dst->m_user);
    safe_free(dst->m_password);

    dst->m_primary   = m_primary;
    dst->m_host      = m_host ? strdup(m_host) : nullptr;
    dst->m_port      = m_port;
    dst->m_user      = m_user ? strdup(m_user) : nullptr;
    dst->m_password  = m_password ? strdup(m_password) : nullptr;
    dst->m_enonceSub = m_enonceSub;
    dst->m_tls       = m_tls;
}


/*
void StratumConfig::toLog(const StratumConfig &cfg, const char* prefix) {
    char c = cfg.m_primary ? 'P' : 'S';
    ESP_LOGE(TAG, "%s [%c] host: %s", prefix, c, cfg.m_host ? cfg.m_host : "null");
    ESP_LOGE(TAG, "%s [%c] port: %d", prefix, c, cfg.m_port);
    ESP_LOGE(TAG, "%s [%c] user: %s", prefix, c, cfg.m_user ? cfg.m_user : "null");
    ESP_LOGE(TAG, "%s [%c] pass: %s", prefix, c, cfg.m_password ? cfg.m_password : "null");
    ESP_LOGE(TAG, "%s [%c] esub:  %s", prefix, c, cfg.m_enonceSub ? "true" : "false");
}
*/