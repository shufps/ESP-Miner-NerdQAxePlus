#include "handler_v2_settings.h"

#include <math.h>
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"
#include "macros.h"

static const char *TAG = "http_v2_settings";

esp_err_t GET_V2_settings(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    Board* board = SYSTEM_MODULE.getBoard();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- device identity ---
    doc["asicModel"]   = board->getAsicModel();
    doc["deviceModel"] = board->getDeviceModel();
    doc["version"]     = esp_app_get_description()->version;
    doc["otp"]         = Config::isOTPEnabled();

    // --- can ---
    {
        JsonObject can = doc["can"].to<JsonObject>();
        can["hasExtension"] = board->hasCanExtension();
        can["enabled"]      = Config::isCanEnabled();
    }

    // --- asic settings (current + defaults + options merged from /asic endpoint) ---
    doc["frequency"]        = board->getAsicFrequency();
    doc["coreVoltage"]      = board->getAsicVoltageMillis();
    doc["vrFrequency"]      = board->getVrFrequency();
    doc["defaultFrequency"] = board->getDefaultAsicFrequency();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["defaultVrFrequency"] = board->getDefaultVrFrequency();
    doc["ecoFrequency"]     = board->getEcoAsicFrequency();
    doc["ecoCoreVoltage"]   = board->getEcoAsicVoltageMillis();
    {
        JsonArray arr = doc["frequencyOptions"].to<JsonArray>();
        for (uint32_t f : board->getFrequencyOptions()) arr.add(f);
    }
    {
        JsonArray arr = doc["voltageOptions"].to<JsonArray>();
        for (uint32_t v : board->getVoltageOptions()) arr.add(v);
    }

    // --- stratum / pools ---
    doc["poolMode"]        = Config::getPoolMode();
    doc["poolBalance"]     = Config::getPoolBalance();
    doc["stratumKeep"]    = Config::isStratumKeepaliveEnabled() ? 1 : 0;
    doc["jobInterval"]     = board->getAsicJobIntervalMs();
    doc["stratumDifficulty"] = Config::getStratumDifficulty();
    {
        JsonArray pools = doc["pools"].to<JsonArray>();

        // Pool 0 — primary
        {
            JsonObject pool = pools.add<JsonObject>();
            char *url  = Config::getStratumURL();
            char *user = Config::getStratumUser();
            pool["url"]              = url  ? url  : "";
            pool["port"]             = Config::getStratumPortNumber();
            pool["user"]             = user ? user : "";
            pool["enonceSubscribe"]  = Config::isStratumEnonceSubscribe();
            pool["tls"]              = Config::isStratumTLS();
            pool["protocol"]         = Config::getStratumProtocol();
            char *sv2 = Config::getSV2AuthorityPubkey();
            pool["sv2AuthorityPubkey"] = sv2 ? sv2 : "";
            safe_free(sv2);
            pool["sv2ChannelType"]   = Config::getSV2ChannelType();
            pool["coinbaseVerifyMode"]  = Config::getCoinbaseVerifyMode(0);
            pool["coinbaseMaxFee"]      = Config::getCoinbaseMaxFee(0) / 10.0f;
            pool["coinbaseVerifyForce"] = Config::getCoinbaseVerifyForce(0);
            free(url);
            free(user);
        }

        // Pool 1 — fallback
        {
            JsonObject pool = pools.add<JsonObject>();
            char *url  = Config::getStratumFallbackURL();
            char *user = Config::getStratumFallbackUser();
            pool["url"]              = url  ? url  : "";
            pool["port"]             = Config::getStratumFallbackPortNumber();
            pool["user"]             = user ? user : "";
            pool["enonceSubscribe"]  = Config::isStratumFallbackEnonceSubscribe();
            pool["tls"]              = Config::isStratumFallbackTLS();
            pool["protocol"]         = Config::getFallbackStratumProtocol();
            char *sv2 = Config::getFallbackSV2AuthorityPubkey();
            pool["sv2AuthorityPubkey"] = sv2 ? sv2 : "";
            safe_free(sv2);
            pool["sv2ChannelType"]   = Config::getFallbackSV2ChannelType();
            pool["coinbaseVerifyMode"]  = Config::getCoinbaseVerifyMode(1);
            pool["coinbaseMaxFee"]      = Config::getCoinbaseMaxFee(1) / 10.0f;
            pool["coinbaseVerifyForce"] = Config::getCoinbaseVerifyForce(1);
            free(url);
            free(user);
        }
    }

    // --- fans ---
    {
        JsonArray fans = doc["fans"].to<JsonArray>();
        int numFans = board->getNumFans();
        for (int ch = 0; ch < numFans; ch++) {
            PidSettings* fanPid = board->getPidSettings(ch);
            JsonObject fan = fans.add<JsonObject>();
            fan["label"]       = board->getFanLabel(ch);
            fan["mode"]        = Config::getFanMode(ch);
            fan["manualSpeed"] = Config::getFanManualSpeed(ch);
            fan["overheatTemp"] = Config::getFanOverheatTemp(ch);
            JsonObject pid_obj = fan["pid"].to<JsonObject>();
            pid_obj["targetTemp"] = board->isPIDAvailable() ? (int) fanPid->targetTemp : -1;
            pid_obj["p"]          = (float) fanPid->p / 100.0f;
            pid_obj["i"]          = (float) fanPid->i / 100.0f;
            pid_obj["d"]          = (float) fanPid->d / 100.0f;
        }
    }
    doc["invertFanPolarity"] = board->isInvertFanPolarityEnabled() ? 1 : 0;

    // --- network ---
    {
        char *hostname = Config::getHostname();
        char *ssid     = Config::getWifiSSID();
        doc["hostname"] = hostname ? hostname : "";
        doc["ssid"]     = ssid     ? ssid     : "";
        free(hostname);
        free(ssid);
    }

    // --- display ---
    doc["flipScreen"]    = board->isFlipScreenEnabled() ? 1 : 0;
    doc["invertScreen"]  = Config::isInvertScreenEnabled() ? 1 : 0;
    doc["autoScreenOff"] = Config::isAutoScreenOffEnabled() ? 1 : 0;

    return sendJsonResponse(req, doc);
}

// ---------------------------------------------------------------------------
// PATCH /api/v2/settings
// ---------------------------------------------------------------------------
// Accepts the same structure as GET /api/v2/settings (all fields optional).
// Pool settings use a pools[] array instead of flat fallback* prefixes.

esp_err_t PATCH_V2_settings(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) {
        return err;
    }

    // --- network ---
    if (doc["ssid"].is<const char*>()) {
        Config::setWifiSSID(doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        Config::setWifiPass(doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        Config::setHostname(doc["hostname"].as<const char*>());
    }

    // --- ASIC settings ---
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t v = doc["coreVoltage"].as<uint16_t>();
        if (v > 0) Config::setAsicVoltage(v);
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t f = doc["frequency"].as<uint16_t>();
        if (f > 0) Config::setAsicFrequency(f);
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t ji = doc["jobInterval"].as<uint16_t>();
        if (ji > 0) Config::setAsicJobInterval(ji);
    }
    if (doc["stratumDifficulty"].is<uint32_t>()) {
        Config::setStratumDifficulty(doc["stratumDifficulty"].as<uint32_t>());
    }
    if (doc["vrFrequency"].is<uint32_t>()) {
        Config::setVrFrequency(doc["vrFrequency"].as<uint32_t>());
    }

    // --- display ---
    if (doc["flipScreen"].is<bool>()) {
        Config::setFlipScreen(doc["flipScreen"].as<bool>());
    }
    if (doc["invertScreen"].is<bool>()) {
        Config::setInvertScreen(doc["invertScreen"].as<bool>());
    }
    if (doc["autoScreenOff"].is<bool>()) {
        Config::setAutoScreenOff(doc["autoScreenOff"].as<bool>());
    }
    if (doc["invertFanPolarity"].is<bool>()) {
        Config::setFanPolarity(doc["invertFanPolarity"].as<bool>());
    }

    // --- misc ---
    if (doc["stratumKeep"].is<bool>() || doc["stratumKeep"].is<int>()) {
        bool value = doc["stratumKeep"].as<int>() != 0;
        Config::setStratumKeepaliveEnabled(value);
    }
    if (doc["canMaster"].is<bool>() || doc["canMaster"].is<int>()) {
        bool value = doc["canMaster"].as<int>() != 0;
        Config::setCanEnabled(value);
    }

    // --- fans[] ---
    if (doc["fans"].is<JsonArray>()) {
        JsonArray fans = doc["fans"].as<JsonArray>();
        int ch = 0;
        for (JsonObject fan : fans) {
            if (ch > 1) break;
            if (fan["mode"].is<uint16_t>())
                Config::setFanMode(ch, fan["mode"].as<uint16_t>());
            if (fan["manualSpeed"].is<uint16_t>())
                Config::setFanManualSpeed(ch, fan["manualSpeed"].as<uint16_t>());
            if (fan["overheatTemp"].is<uint16_t>())
                Config::setFanOverheatTemp(ch, fan["overheatTemp"].as<uint16_t>());
            if (fan["pid"].is<JsonObject>()) {
                JsonObject p = fan["pid"].as<JsonObject>();
                if (p["targetTemp"].is<uint16_t>())
                    Config::setFanPidTargetTemp(ch, p["targetTemp"].as<uint16_t>());
                if (p["p"].is<float>())
                    Config::setFanPidP(ch, (uint16_t)(p["p"].as<float>() * 100.0f));
                if (p["i"].is<float>())
                    Config::setFanPidI(ch, (uint16_t)(p["i"].as<float>() * 100.0f));
                if (p["d"].is<float>())
                    Config::setFanPidD(ch, (uint16_t)(p["d"].as<float>() * 100.0f));
            }
            ch++;
        }
    }

    // --- pools[] ---
    if (doc["poolMode"].is<uint16_t>()) {
        Config::setPoolMode(doc["poolMode"].as<uint16_t>());
    }
    if (doc["poolBalance"].is<uint16_t>()) {
        Config::setPoolBalance(doc["poolBalance"].as<uint16_t>());
    }

    bool verifyChanged[2] = {false, false};

    if (doc["pools"].is<JsonArray>()) {
        JsonArray pools = doc["pools"].as<JsonArray>();

        for (int i = 0; i < (int)pools.size() && i < 2; i++) {
            JsonObject pool = pools[i].as<JsonObject>();

            if (pool["url"].is<const char*>()) {
                if (i == 0) Config::setStratumURL(pool["url"].as<const char*>());
                else        Config::setStratumFallbackURL(pool["url"].as<const char*>());
            }
            if (pool["port"].is<uint16_t>()) {
                if (i == 0) Config::setStratumPortNumber(pool["port"].as<uint16_t>());
                else        Config::setStratumFallbackPortNumber(pool["port"].as<uint16_t>());
            }
            if (pool["user"].is<const char*>()) {
                if (i == 0) Config::setStratumUser(pool["user"].as<const char*>());
                else        Config::setStratumFallbackUser(pool["user"].as<const char*>());
            }
            if (pool["password"].is<const char*>()) {
                if (i == 0) Config::setStratumPass(pool["password"].as<const char*>());
                else        Config::setStratumFallbackPass(pool["password"].as<const char*>());
            }
            if (pool["enonceSubscribe"].is<bool>()) {
                if (i == 0) Config::setStratumEnonceSubscribe(pool["enonceSubscribe"].as<bool>());
                else        Config::setStratumFallbackEnonceSubscribe(pool["enonceSubscribe"].as<bool>());
            }
            if (pool["tls"].is<bool>()) {
                if (i == 0) Config::setStratumTLS(pool["tls"].as<bool>());
                else        Config::setStratumFallbackTLS(pool["tls"].as<bool>());
            }
            if (pool["protocol"].is<uint16_t>()) {
                if (i == 0) Config::setStratumProtocol(pool["protocol"].as<uint16_t>());
                else        Config::setFallbackStratumProtocol(pool["protocol"].as<uint16_t>());
            }
            if (pool["sv2AuthorityPubkey"].is<const char*>()) {
                if (i == 0) Config::setSV2AuthorityPubkey(pool["sv2AuthorityPubkey"].as<const char*>());
                else        Config::setFallbackSV2AuthorityPubkey(pool["sv2AuthorityPubkey"].as<const char*>());
            }
            if (pool["sv2ChannelType"].is<uint16_t>()) {
                if (i == 0) Config::setSV2ChannelType(pool["sv2ChannelType"].as<uint16_t>());
                else        Config::setFallbackSV2ChannelType(pool["sv2ChannelType"].as<uint16_t>());
            }

            // Coinbase verification
            if (pool["coinbaseVerifyMode"].is<uint16_t>()) {
                verifyChanged[i] |= Config::getCoinbaseVerifyMode(i) != pool["coinbaseVerifyMode"].as<uint16_t>();
                Config::setCoinbaseVerifyMode(i, pool["coinbaseVerifyMode"].as<uint16_t>());
            }
            if (pool["coinbaseMaxFee"].is<float>()) {
                uint16_t newVal = (uint16_t)roundf(pool["coinbaseMaxFee"].as<float>() * 10.0f);
                verifyChanged[i] |= Config::getCoinbaseMaxFee(i) != newVal;
                Config::setCoinbaseMaxFee(i, newVal);
            }
            if (pool["coinbaseVerifyForce"].is<bool>()) {
                verifyChanged[i] |= Config::getCoinbaseVerifyForce(i) != pool["coinbaseVerifyForce"].as<bool>();
                Config::setCoinbaseVerifyForce(i, pool["coinbaseVerifyForce"].as<bool>());
            }
        }
    }

    // Re-run verification for changed pools
    for (int i = 0; i < 2; i++) {
        if (verifyChanged[i]) {
            STRATUM_MANAGER->clearVerifyBlocked(i);
            STRATUM_MANAGER->resetVerificationStats(i);
        }
        STRATUM_MANAGER->rerunVerification(i);
    }
    if (SYSTEM_MODULE.getBoardError() == Board::Error::COINBASE_VERIFY_FAULT &&
        !STRATUM_MANAGER->isVerifyBlocked(0) && !STRATUM_MANAGER->isVerifyBlocked(1)) {
        SYSTEM_MODULE.clearBoardError();
    }

    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);

    // Reload all subsystems
    Board *board = SYSTEM_MODULE.getBoard();
    board->loadSettings();
    POWER_MANAGEMENT_MODULE.getFanController().loadSettings();
    SYSTEM_MODULE.loadSettings();
    STRATUM_MANAGER->loadSettings();

    return ESP_OK;
}
