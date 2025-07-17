#include "esp_ota_ops.h"
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

static const char *TAG = "http_system";

#define VR_FREQUENCY_ENABLED

/* Simple handler for getting system handler */
esp_err_t GET_system_info(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Parse optional start_timestamp parameter
    uint64_t start_timestamp = 0;
    uint64_t current_timestamp = 0;
    bool history_requested = false;
    char query_str[128];
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        char param[64];
        if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
            start_timestamp = strtoull(param, NULL, 10);
            if (start_timestamp) {
                history_requested = true;
            }
        }
        if (httpd_query_key_value(query_str, "cur", param, sizeof(param)) == ESP_OK) {
            current_timestamp = strtoull(param, NULL, 10);
            ESP_LOGI(TAG, "cur: %llu", current_timestamp);
        }
    }

    Board* board   = SYSTEM_MODULE.getBoard();
    History* history = SYSTEM_MODULE.getHistory();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Get configuration strings from NVS
    char *ssid               = Config::getWifiSSID();
    char *hostname           = Config::getHostname();
    char *stratumURL         = Config::getStratumURL();
    char *stratumUser        = Config::getStratumUser();
    char *fallbackStratumURL = Config::getStratumFallbackURL();
    char *fallbackStratumUser= Config::getStratumFallbackUser();

    // static
    doc["asicCount"]          = board->getAsicCount();
    doc["smallCoreCount"]     = (board->getAsics()) ? board->getAsics()->getSmallCoreCount() : 0;
    doc["deviceModel"]        = board->getDeviceModel();
    doc["hostip"]             = SYSTEM_MODULE.getIPAddress();
    doc["macAddr"]            = SYSTEM_MODULE.getMacAddress();
    doc["wifiRSSI"]           = SYSTEM_MODULE.get_wifi_rssi();

    // dashboard
    doc["power"]              = POWER_MANAGEMENT_MODULE.getPower();
    doc["maxPower"]           = board->getMaxPin();
    doc["minPower"]           = board->getMinPin();
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["maxVoltage"]         = board->getMaxVin();
    doc["minVoltage"]         = board->getMinVin();
    doc["current"]            = POWER_MANAGEMENT_MODULE.getCurrent();
    doc["temp"]               = POWER_MANAGEMENT_MODULE.getChipTempMax();
    doc["vrTemp"]             = POWER_MANAGEMENT_MODULE.getVRTemp();
    doc["hashRateTimestamp"]  = history->getCurrentTimestamp();
    doc["hashRate"]           = SYSTEM_MODULE.getCurrentHashrate();
    doc["hashRate_1m"]        = history->getCurrentHashrate1m();
    doc["hashRate_10m"]       = history->getCurrentHashrate10m();
    doc["hashRate_1h"]        = history->getCurrentHashrate1h();
    doc["hashRate_1d"]        = history->getCurrentHashrate1d();
    doc["bestDiff"]           = SYSTEM_MODULE.getBestDiffString();
    doc["bestSessionDiff"]    = SYSTEM_MODULE.getBestSessionDiffString();
    doc["coreVoltage"]        = board->getAsicVoltageMillis();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["coreVoltageActual"]  = (int) (board->getVout() * 1000.0f);
    doc["sharesAccepted"]     = SYSTEM_MODULE.getSharesAccepted();
    doc["sharesRejected"]     = SYSTEM_MODULE.getSharesRejected();
    doc["duplicateHWNonces"]  = SYSTEM_MODULE.getDuplicateHWNonces();
    doc["isUsingFallbackStratum"] = STRATUM_MANAGER.isUsingFallback();
    doc["isStratumConnected"] = STRATUM_MANAGER.isAnyConnected();
    doc["fanspeed"]           = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["fanrpm"]             = POWER_MANAGEMENT_MODULE.getFanRPM();
    doc["lastpingrtt"]        = get_last_ping_rtt();
    doc["recentpingloss"]     = get_recent_ping_loss();
    doc["poolDifficulty"]     = SYSTEM_MODULE.getPoolDifficulty();
    doc["foundBlocks"]        = SYSTEM_MODULE.getFoundBlocks();
    doc["totalFoundBlocks"]   = SYSTEM_MODULE.getTotalFoundBlocks();

    // asic temps
    {
        JsonArray arr = doc["asicTemps"].to<JsonArray>();
        for (int i=0;i<board->getAsicCount();i++) {
            arr.add(board->getChipTemp(i));
        }
    }

    // If history was requested, add the history data as a nested object
    if (history_requested) {
        uint64_t end_timestamp = start_timestamp + 3600 * 1000ULL; // 1 hour later
        JsonObject json_history = doc["history"].to<JsonObject>();

        History *history = SYSTEM_MODULE.getHistory();
        history->exportHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp);
    }

    // settings
    PidSettings *pid = board->getPidSettings();
    doc["pidTargetTemp"]      = board->isPIDAvailable() ? pid->targetTemp : -1;
    doc["pidP"]               = (float) pid->p / 100.0f;
    doc["pidI"]               = (float) pid->i / 100.0f;
    doc["pidD"]               = (float) pid->d / 100.0f;

    doc["hostname"]           = hostname;
    doc["ssid"]               = ssid;
    doc["stratumURL"]         = stratumURL;
    doc["stratumPort"]        = Config::getStratumPortNumber();
    doc["stratumUser"]        = stratumUser;
    doc["stratumEnonceSubscribe"] = Config::isStratumEnonceSubscribe();
    doc["fallbackStratumURL"] = fallbackStratumURL;
    doc["fallbackStratumPort"]= Config::getStratumFallbackPortNumber();
    doc["fallbackStratumUser"] = fallbackStratumUser;
    doc["fallbackStratumEnonceSubscribe"] = Config::isStratumFallbackEnonceSubscribe();
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["frequency"]          = board->getAsicFrequency();
    doc["defaultFrequency"]   = board->getDefaultAsicFrequency();
    doc["jobInterval"]        = board->getAsicJobIntervalMs();
    doc["stratumDifficulty"] = Config::getStratumDifficulty();
    doc["overheat_temp"]      = Config::getOverheatTemp();
    doc["flipscreen"]         = board->isFlipScreenEnabled() ? 1 : 0;
    doc["invertscreen"]       = Config::isInvertScreenEnabled() ? 1 : 0; // unused?
    doc["autoscreenoff"]      = Config::isAutoScreenOffEnabled() ? 1 : 0;
    doc["invertfanpolarity"]  = board->isInvertFanPolarityEnabled() ? 1 : 0;
    doc["autofanpolarity"]  = board->isAutoFanPolarityEnabled() ? 1 : 0;
    doc["autofanspeed"]       = Config::getTempControlMode();
    doc["stratum_keep"]       = Config::isStratumKeepaliveEnabled() ? 1 : 0;
#ifdef VR_FREQUENCY_ENABLED
    doc["vrFrequency"]        = board->getVrFrequency();
    doc["defaultVrFrequency"] = board->getDefaultVrFrequency();
#endif
    doc["otp"]                = Config::isOTPEnabled(); // flag if otp is enabled

    // system screen
    doc["ASICModel"]          = board->getAsicModel();
    doc["uptimeSeconds"]      = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]    = SYSTEM_MODULE.getLastResetReason();
    doc["wifiStatus"]         = SYSTEM_MODULE.getWifiStatus();
    doc["freeHeap"]           = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    doc["freeHeapInt"]        = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    doc["version"]            = esp_app_get_description()->version;
    doc["runningPartition"]   = esp_ota_get_running_partition()->label;

    doc["defaultTheme"]       = board->getDefaultTheme();

    //ESP_LOGI(TAG, "allocs: %d, deallocs: %d, reallocs: %d", allocs, deallocs, reallocs);

    // close connection to prevent clogging
    httpd_resp_set_hdr(req, "Connection", "close");

    // Serialize the JSON document to a String and send it
    esp_err_t ret = sendJsonResponse(req, doc);
    doc.clear();

    // Free temporary strings
    free(ssid);
    free(hostname);
    free(stratumURL);
    free(stratumUser);
    free(fallbackStratumURL);
    free(fallbackStratumUser);

    return ret;
}



esp_err_t PATCH_update_settings(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
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

    // Update settings if each key exists in the JSON object.
    if (doc["stratumURL"].is<const char*>()) {
        Config::setStratumURL(doc["stratumURL"].as<const char*>());
    }
    if (doc["stratumUser"].is<const char*>()) {
        Config::setStratumUser(doc["stratumUser"].as<const char*>());
    }
    if (doc["stratumPassword"].is<const char*>()) {
        Config::setStratumPass(doc["stratumPassword"].as<const char*>());
    }
    if (doc["stratumPort"].is<uint16_t>()) {
        Config::setStratumPortNumber(doc["stratumPort"].as<uint16_t>());
    }
    if (doc["stratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumEnonceSubscribe(doc["stratumEnonceSubscribe"].as<bool>());
    }
    if (doc["fallbackStratumURL"].is<const char*>()) {
        Config::setStratumFallbackURL(doc["fallbackStratumURL"].as<const char*>());
    }
    if (doc["fallbackStratumUser"].is<const char*>()) {
        Config::setStratumFallbackUser(doc["fallbackStratumUser"].as<const char*>());
    }
    if (doc["fallbackStratumPassword"].is<const char*>()) {
        Config::setStratumFallbackPass(doc["fallbackStratumPassword"].as<const char*>());
    }
    if (doc["fallbackStratumPort"].is<uint16_t>()) {
        Config::setStratumFallbackPortNumber(doc["fallbackStratumPort"].as<uint16_t>());
    }
    if (doc["fallbackStratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumFallbackEnonceSubscribe(doc["fallbackStratumEnonceSubscribe"].as<bool>());
    }
    if (doc["ssid"].is<const char*>()) {
        Config::setWifiSSID(doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        Config::setWifiPass(doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        Config::setHostname(doc["hostname"].as<const char*>());
    }
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t coreVoltage = doc["coreVoltage"].as<uint16_t>();
        if (coreVoltage > 0) {
            Config::setAsicVoltage(coreVoltage);
        }
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t frequency = doc["frequency"].as<uint16_t>();
        if (frequency > 0) {
            Config::setAsicFrequency(frequency);
        }
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t jobInterval = doc["jobInterval"].as<uint16_t>();
        if (jobInterval > 0) {
            Config::setAsicJobInterval(jobInterval);
        }
    }
    if (doc["stratumDifficulty"].is<uint32_t>()) {
        Config::setStratumDifficulty(doc["stratumDifficulty"].as<uint32_t>());
    }
    if (doc["flipscreen"].is<bool>()) {
        Config::setFlipScreen(doc["flipscreen"].as<bool>());
    }
    if (doc["overheat_temp"].is<uint16_t>()) {
        Config::setOverheatTemp(doc["overheat_temp"].as<uint16_t>());
    }
    if (doc["invertscreen"].is<bool>()) {
        Config::setInvertScreen(doc["invertscreen"].as<bool>());
    }
    if (doc["invertfanpolarity"].is<bool>()) {
        Config::setInvertFanPolarity(doc["invertfanpolarity"].as<bool>());
    }
    if (doc["autofanpolarity"].is<bool>()) {
        Config::setAutoFanPolarity(doc["autofanpolarity"].as<bool>());
    }
    if (doc["autofanspeed"].is<uint16_t>()) {
        Config::setTempControlMode(doc["autofanspeed"].as<uint16_t>());
    }
    if (doc["fanspeed"].is<uint16_t>()) {
        Config::setFanSpeed(doc["fanspeed"].as<uint16_t>());
    }
    if (doc["autoscreenoff"].is<bool>()) {
        Config::setAutoScreenOff(doc["autoscreenoff"].as<bool>());
    }
    if (doc["stratum_keep"].is<bool>() || doc["stratum_keep"].is<int>()) {
        bool value = doc["stratum_keep"].as<int>() != 0;
        Config::setStratumKeepaliveEnabled(value);
        ESP_LOGI("system", "stratum_keep updated via WebUI: %s", value ? "ENABLED" : "DISABLED");
    }
    if (doc["pidTargetTemp"].is<uint16_t>()) {
        Config::setPidTargetTemp(doc["pidTargetTemp"].as<uint16_t>());
    }
    if (doc["pidP"].is<float>()) {
        Config::setPidP((uint16_t) (doc["pidP"].as<float>() * 100.0f));
    }
    if (doc["pidI"].is<float>()) {
        Config::setPidI((uint16_t) (doc["pidI"].as<float>() * 100.0f));
    }
    if (doc["pidD"].is<float>()) {
        Config::setPidD((uint16_t) (doc["pidD"].as<float>() * 100.0f));
    }
#ifdef VR_FREQUENCY_ENABLED
    if (doc["vrFrequency"].is<uint32_t>()) {
        Config::setVrFrequency(doc["vrFrequency"].as<uint32_t>());
    }
#endif



    doc.clear();

    // Apply all changes to NVS in one go
    Config::flush_nvs_changes();
    ESP_LOGI(TAG, "Configuration changes flushed to NVS");

    // Signal the end of the response
    httpd_resp_send_chunk(req, NULL, 0);

    // Reload settings after update
    Board* board = SYSTEM_MODULE.getBoard();
    board->loadSettings();

    // reload settings of system module (and display)
    SYSTEM_MODULE.loadSettings();

    return ESP_OK;
}

esp_err_t GET_system_asic(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // CORS
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    Board* board = SYSTEM_MODULE.getBoard();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Basisfelder
    doc["ASICModel"]        = board->getAsicModel();
    doc["deviceModel"]      = board->getDeviceModel();
    doc["asicCount"]        = board->getAsicCount();
    doc["defaultFrequency"] = board->getDefaultAsicFrequency();
    doc["defaultVoltage"]   = board->getDefaultAsicVoltageMillis();
    doc["absMaxFrequency"]  = board->getAbsMaxAsicFrequency();
    doc["absMaxVoltage"]    = board->getAbsMaxAsicVoltageMillis();
    doc["ecoFrequency"]     = board->getEcoAsicFrequency();
    doc["ecoVoltage"]       = board->getEcoAsicVoltageMillis();

    doc["swarmColor"]       = board->getSwarmColorName();

    // frequencyOptions
    {
        JsonArray arr = doc["frequencyOptions"].to<JsonArray>();
        const auto& freqs = board->getFrequencyOptions();
        for (uint32_t f : freqs) { arr.add(f); }
    }

    // voltageOptions
    {
        JsonArray arr = doc["voltageOptions"].to<JsonArray>();
        const auto& volts = board->getVoltageOptions();
        for (uint32_t v : volts) { arr.add(v); }
    }

    // Verbindung schließen, damit nichts „hängt“
    httpd_resp_set_hdr(req, "Connection", "close");

    esp_err_t ret = sendJsonResponse(req, doc);
    doc.clear();
    return ret;
}
