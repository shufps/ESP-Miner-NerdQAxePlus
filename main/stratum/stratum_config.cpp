#pragma once
#include "esp_log.h"
#include "stratum_config.h"
#include "ArduinoJson.h"
#include "nvs_config.h"
#include "psram_allocator.h"

static const char *TAG="StratumConfig";

bool StratumConfig::isEqual(const StratumConfig &other)
{
    if (!m_host || !m_user || !m_password || !other.m_host || !other.m_user || !other.m_password) {
        return false;
    }
    bool ret = (m_primary == other.m_primary);
    ret = ret && (!strcmp(m_host, other.m_host));
    ret = ret && (m_port == other.m_port);
    ret = ret && (!strcmp(m_user, other.m_user));
    ret = ret && (!strcmp(m_user, other.m_password));
    ret = ret && (m_enonceSub == other.m_enonceSub);
    return ret;
}


StratumConfig StratumConfig::readPrimary()
{
    StratumConfig cfg{};
    cfg.m_primary = true;
    cfg.m_host = Config::getStratumURL();
    cfg.m_port = Config::getStratumPortNumber();
    cfg.m_user = Config::getStratumUser();
    cfg.m_password = Config::getStratumPass();
    cfg.m_enonceSub = Config::isStratumEnonceSubscribe();
    return cfg;
}

StratumConfig StratumConfig::readFallback()
{
    StratumConfig cfg{};
    cfg.m_primary = false;
    cfg.m_host = Config::getStratumFallbackURL();
    cfg.m_port = Config::getStratumFallbackPortNumber();
    cfg.m_user = Config::getStratumFallbackUser();
    cfg.m_password = Config::getStratumFallbackPass();
    cfg.m_enonceSub = Config::isStratumFallbackEnonceSubscribe();
    return cfg;
}

StratumConfig StratumConfig::read(int pool)
{
    return !pool ? readPrimary() : readFallback();
}

void StratumConfig::toLog(const StratumConfig &cfg, const char* prefix) {
    char c = cfg.m_primary ? 'P' : 'S';
    ESP_LOGE(TAG, "%s [%c] host: %s", prefix, c, cfg.m_host ? cfg.m_host : "null");
    ESP_LOGE(TAG, "%s [%c] port: %d", prefix, c, cfg.m_port);
    ESP_LOGE(TAG, "%s [%c] user: %s", prefix, c, cfg.m_user ? cfg.m_user : "null");
    ESP_LOGE(TAG, "%s [%c] pass: %s", prefix, c, cfg.m_password ? cfg.m_password : "null");
    ESP_LOGE(TAG, "%s [%c] esub:  %s", prefix, c, cfg.m_enonceSub ? "true" : "false");
}