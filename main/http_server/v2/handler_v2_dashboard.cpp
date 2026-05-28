#include "handler_v2_dashboard.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"
#include "ping_task.h"
#include "tasks/can_master_task.h"

static const char *TAG = "http_v2_dashboard";

esp_err_t GET_V2_dashboard(httpd_req_t *req)
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

    // Parse optional history query params (same as /api/system/info)
    const uint64_t DEFAULT_HISTORY_SPAN_MS = 3600ULL * 1000ULL;
    const uint64_t MAX_HISTORY_SPAN_MS     = 3ULL * 3600ULL * 1000ULL;
    uint64_t start_timestamp   = 0;
    uint64_t current_timestamp = 0;
    uint32_t history_limit     = 0;
    bool     history_requested = false;
    uint64_t history_span_ms   = DEFAULT_HISTORY_SPAN_MS;
    char     query_str[128];
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        char param[64];
        if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
            start_timestamp = strtoull(param, NULL, 10);
            if (start_timestamp) history_requested = true;
        }
        if (httpd_query_key_value(query_str, "limit", param, sizeof(param)) == ESP_OK) {
            history_limit = strtoul(param, NULL, 10);
            if (history_limit > 1000) history_limit = 1000;
        }
        if (httpd_query_key_value(query_str, "historySpan", param, sizeof(param)) == ESP_OK) {
            history_span_ms = strtoull(param, NULL, 10);
            if (history_span_ms > MAX_HISTORY_SPAN_MS) history_span_ms = MAX_HISTORY_SPAN_MS;
            if (history_span_ms == 0) history_span_ms = DEFAULT_HISTORY_SPAN_MS;
        }
        if (httpd_query_key_value(query_str, "cur", param, sizeof(param)) == ESP_OK) {
            current_timestamp = strtoull(param, NULL, 10);
        }
    }

    Board*   board   = SYSTEM_MODULE.getBoard();
    History* history = SYSTEM_MODULE.getHistory();
    bool     shutdown = POWER_MANAGEMENT_MODULE.isShutdown();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- system ---
    {
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["uptime"]       = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
        sys["shutdown"]     = shutdown;
        sys["boardError"]   = (int) SYSTEM_MODULE.getBoardError();
        sys["overheatTemp"] = Config::getOverheatTemp();
    }

    // --- performance ---
    {
        JsonObject perf = doc["performance"].to<JsonObject>();
        perf["hashRateTimestamp"] = history->getCurrentTimestamp();
        perf["hashRate"]     = !shutdown ? SYSTEM_MODULE.getCurrentHashrate()    : 0.0;
        perf["hashRate1m"]  = !shutdown ? history->getCurrentHashrate1m()       : 0.0;
        perf["hashRate10m"] = !shutdown ? history->getCurrentHashrate10m()      : 0.0;
        perf["hashRate1h"]  = !shutdown ? history->getCurrentHashrate1h()       : 0.0;
        perf["hashRate1d"]  = !shutdown ? history->getCurrentHashrate1d()       : 0.0;
        perf["bestDiff"]        = STRATUM_MANAGER->getBestDiff();
        perf["bestSessionDiff"] = STRATUM_MANAGER->getBestSessionDiff();
        perf["sharesAccepted"]  = STRATUM_MANAGER->getSharesAccepted();
        perf["sharesRejected"]  = STRATUM_MANAGER->getSharesRejected();
        perf["frequency"]       = board->getAsicFrequency();
        perf["asicCount"]       = board->getAsicCount();
        perf["smallCoreCount"]  = board->getAsics() ? board->getAsics()->getSmallCoreCount() : 0;
    }

    // --- power ---
    {
        JsonObject pwr = doc["power"].to<JsonObject>();
        pwr["watts"]             = POWER_MANAGEMENT_MODULE.getPower();
        pwr["min"]               = board->getMinPin();
        pwr["max"]               = board->getMaxPin();
        pwr["voltage"]           = POWER_MANAGEMENT_MODULE.getVoltage() / 1000.0f; // V
        pwr["voltageMin"]        = board->getMinVin();
        pwr["voltageMax"]        = board->getMaxVin();
        pwr["currentA"]          = POWER_MANAGEMENT_MODULE.getCurrent() / 1000.0f; // A
        pwr["currentAMin"]       = board->getMinCurrentA();
        pwr["currentAMax"]       = board->getMaxCurrentA();
        pwr["coreVoltageActual"] = board->getVout(); // V
    }

    // --- thermal ---
    {
        JsonObject therm = doc["thermal"].to<JsonObject>();
        therm["asicTemp"]  = POWER_MANAGEMENT_MODULE.getChipTempMax();
        therm["vrTemp"]    = POWER_MANAGEMENT_MODULE.getVRTemp();
        therm["vrTempInt"] = POWER_MANAGEMENT_MODULE.getVRTempInt();
        {
            JsonArray asic_temps = therm["asicTemps"].to<JsonArray>();
            for (int i = 0; i < board->getAsicCount(); i++) {
                asic_temps.add(board->getChipTemp(i));
            }
        }
        {
            JsonArray fans = therm["fans"].to<JsonArray>();
            for (int ch = 0; ch < board->getNumFans(); ch++) {
                JsonObject fan = fans.add<JsonObject>();
                fan["speed"] = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
                fan["rpm"]   = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
            }
        }
    }

    // --- stratum ---
    {
        JsonObject stratum = doc["stratum"].to<JsonObject>();
        STRATUM_MANAGER->getManagerInfoJson(stratum);

        // Enrich each pool entry with host / port / user from NVS config
        char *urls[2]  = { Config::getStratumURL(), Config::getStratumFallbackURL() };
        char *users[2] = { Config::getStratumUser(), Config::getStratumFallbackUser() };
        int   ports[2] = { (int) Config::getStratumPortNumber(), (int) Config::getStratumFallbackPortNumber() };

        JsonArray pools = stratum["pools"].as<JsonArray>();
        for (int i = 0; i < (int) pools.size() && i < 2; i++) {
            JsonObject pool = pools[i].as<JsonObject>();
            pool["host"] = urls[i]  ? urls[i]  : "";
            pool["port"] = ports[i];
            pool["user"] = users[i] ? users[i] : "";
        }

        for (int i = 0; i < 2; i++) {
            if (urls[i])  free(urls[i]);
            if (users[i]) free(users[i]);
        }
    }

    // --- can ---
    {
        JsonObject can = doc["can"].to<JsonObject>();
        can["hasExtension"] = board->hasCanExtension();
        can["enabled"]      = Config::isCanEnabled();
    }

    // --- coinbase ---
    {
        JsonObject coinbase = doc["coinbase"].to<JsonObject>();

        JsonArray blockHeaders = coinbase["blockHeaders"].to<JsonArray>();
        for (int p = 0; p < 2; p++) {
            const coinbase_result_t cb = STRATUM_MANAGER->getCoinbaseResult(p);
            if (cb.block_height > 0) {
                JsonObject bh = blockHeaders.add<JsonObject>();
                bh["pool"]              = p;
                bh["blockHeight"]       = cb.block_height;
                bh["networkDifficulty"] = cb.network_difficulty;
                bh["scriptSig"]         = cb.scriptsig;
                if (Config::getCoinbaseVerifyMode(p) > 0) {
                    bh["coinbaseValueTotalSatoshis"] = cb.total_value_satoshis;
                    bh["coinbaseValueUserSatoshis"]  = cb.user_value_satoshis;
                    bh["verificationOk"]         = STRATUM_MANAGER->getVerificationOk(p);
                    bh["verificationFailCount"]  = STRATUM_MANAGER->getVerificationFailCount(p);
                    bh["verificationCheckCount"] = STRATUM_MANAGER->getVerificationCheckCount(p);
                }
            }
        }

        JsonArray cbPools = coinbase["pools"].to<JsonArray>();
        for (int p = 0; p < 2; p++) {
            JsonObject cp = cbPools.add<JsonObject>();
            cp["mode"]   = Config::getCoinbaseVerifyMode(p);
            cp["maxFee"] = Config::getCoinbaseMaxFee(p) / 10.0f;
            cp["force"]  = Config::getCoinbaseVerifyForce(p);
        }
    }

    // --- history (optional, same query params as /api/system/info) ---
    if (!shutdown && history_requested) {
        uint64_t end_timestamp = start_timestamp + history_span_ms;
        JsonObject json_history = doc["history"].to<JsonObject>();
        history->exportHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp, history_limit);
    }

    return sendJsonResponse(req, doc);
}
