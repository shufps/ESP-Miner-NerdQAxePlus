#pragma once

#include "ArduinoJson.h"
#include "nvs_config.h"
#include "psram_allocator.h"

struct StratumConfigView
{
    bool primary;
    char *host;
    int port;
    char *user;
    char *password;
    bool enonceSub;
};

class StratumConfigReader {
  public:
    static void releaseConfig(StratumConfigView &cfg) {
        safe_free(cfg.host);
        safe_free(cfg.user);
        safe_free(cfg.password);
        cfg.port = 0;
        cfg.enonceSub = false;
    }
    static StratumConfigView readPrimary()
    {
        StratumConfigView cfg{};
        cfg.primary = true;
        cfg.host = Config::getStratumURL();
        cfg.port = Config::getStratumPortNumber();
        cfg.user = Config::getStratumUser();
        cfg.password = Config::getStratumPass();
        cfg.enonceSub = Config::isStratumEnonceSubscribe();
        return cfg;
    }

    static StratumConfigView readFallback()
    {
        StratumConfigView cfg{};
        cfg.primary = false;
        cfg.host = Config::getStratumFallbackURL();
        cfg.port = Config::getStratumFallbackPortNumber();
        cfg.user = Config::getStratumFallbackUser();
        cfg.password = Config::getStratumFallbackPass();
        cfg.enonceSub = Config::isStratumFallbackEnonceSubscribe();
        return cfg;
    }
};

class StratumConfigWriter {
  public:
    static bool applyPrimaryPatch(const JsonDocument &doc, const StratumConfigView &oldCfg)
    {
        StratumConfigView cfg = oldCfg; // start from old, override if present
        bool changed = false;

        if (doc["stratumURL"].is<const char *>()) {
            const char *h = doc["stratumURL"].as<const char *>();
            if (!strEqual(cfg.host, h)) {
                Config::setStratumURL(h);
                changed = true;
            }
        }
        if (doc["stratumUser"].is<const char *>()) {
            const char *u = doc["stratumUser"].as<const char *>();
            if (!strEqual(cfg.user, u)) {
                Config::setStratumUser(u);
                changed = true;
            }
        }
        if (doc["stratumPassword"].is<const char *>()) {
            const char *p = doc["stratumPassword"].as<const char *>();

            // if password field is empty -> treat as "no change"
            if (p && p[0] != '\0') {
                if (!strEqual(cfg.password, p)) {
                    Config::setStratumPass(p);
                    changed = true;
                }
            }
            // else: p == nullptr oder "" -> ignore, keep old password
        }
        if (doc["stratumPort"].is<uint16_t>()) {
            int port = doc["stratumPort"].as<uint16_t>();
            if (cfg.port != port) {
                Config::setStratumPortNumber(port);
                changed = true;
            }
        }
        if (doc["stratumEnonceSubscribe"].is<bool>()) {
            bool e = doc["stratumEnonceSubscribe"].as<bool>();
            if (cfg.enonceSub != e) {
                Config::setStratumEnonceSubscribe(e);
                changed = true;
            }
        }

        return changed;
    }

    static bool applyFallbackPatch(const JsonDocument &doc, const StratumConfigView &oldCfg)
    {
        StratumConfigView cfg = oldCfg;
        bool changed = false;

        if (doc["fallbackStratumURL"].is<const char *>()) {
            const char *h = doc["fallbackStratumURL"].as<const char *>();
            if (!strEqual(cfg.host, h)) {
                Config::setStratumFallbackURL(h);
                changed = true;
            }
        }
        if (doc["fallbackStratumUser"].is<const char *>()) {
            const char *u = doc["fallbackStratumUser"].as<const char *>();
            if (!strEqual(cfg.user, u)) {
                Config::setStratumFallbackUser(u);
                changed = true;
            }
        }
        if (doc["fallbackStratumPassword"].is<const char *>()) {
            const char *p = doc["fallbackStratumPassword"].as<const char *>();

            // if password field is empty -> treat as "no change"
            if (p && p[0] != '\0') {
                if (!strEqual(cfg.password, p)) {
                    Config::setStratumFallbackPass(p);
                    changed = true;
                }
            }
            // else: p == nullptr oder "" -> ignore, keep old password
        }
        if (doc["fallbackStratumPort"].is<uint16_t>()) {
            int port = doc["fallbackStratumPort"].as<uint16_t>();
            if (cfg.port != port) {
                Config::setStratumFallbackPortNumber(port);
                changed = true;
            }
        }
        if (doc["fallbackStratumEnonceSubscribe"].is<bool>()) {
            bool e = doc["fallbackStratumEnonceSubscribe"].as<bool>();
            if (cfg.enonceSub != e) {
                Config::setStratumFallbackEnonceSubscribe(e);
                changed = true;
            }
        }

        return changed;
    }

  private:
    static bool strEqual(const char *a, const char *b)
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return strcmp(a, b) == 0;
    }
};
