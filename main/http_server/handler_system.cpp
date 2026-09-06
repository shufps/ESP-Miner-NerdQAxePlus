#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_core_dump.h"
#include "esp_partition.h"

#include <cstring>

#include "ArduinoJson.h"

#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"

#include "ping_task.h"

static const char *TAG = "http_system";

#define VR_FREQUENCY_ENABLED

uint64_t getDuplicateHWNonces();

static esp_err_t getCoreDumpElfSha256(const esp_partition_t *partition, size_t dump_size,
                                     char *scratch, size_t scratch_size,
                                     char *sha256, size_t sha256_size)
{
    static const char NOTE_NAME[] = "ESP_CORE_DUMP_INFO";
    static const uint32_t NOTE_TYPE = 8266;

    struct ElfNoteHeader {
        uint32_t name_size;
        uint32_t description_size;
        uint32_t type;
    };

    const size_t marker_size = sizeof(NOTE_NAME) - 1;
    size_t read_offset = 0;
    size_t marker_offset = SIZE_MAX;

    while (read_offset < dump_size) {
        const size_t read_size = min(dump_size - read_offset, scratch_size);
        esp_err_t err = esp_partition_read(partition, read_offset, scratch, read_size);
        if (err != ESP_OK) {
            return err;
        }

        for (size_t i = 0; i + marker_size <= read_size; i++) {
            if (memcmp(scratch + i, NOTE_NAME, marker_size) == 0) {
                marker_offset = read_offset + i;
                break;
            }
        }
        if (marker_offset != SIZE_MAX || read_size < marker_size) {
            break;
        }

        read_offset += read_size - (marker_size - 1);
    }

    if (marker_offset < sizeof(ElfNoteHeader)) {
        return ESP_ERR_NOT_FOUND;
    }

    ElfNoteHeader note = {};
    const size_t note_offset = marker_offset - sizeof(note);
    esp_err_t err = esp_partition_read(partition, note_offset, &note, sizeof(note));
    if (err != ESP_OK) {
        return err;
    }

    if (note.type != NOTE_TYPE || note.name_size < marker_size + 1 || note.name_size > 32 ||
        note.description_size < sizeof(uint32_t) + 9) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const size_t description_offset = note_offset + sizeof(note) + ((note.name_size + 3) & ~((size_t) 3));
    if (description_offset > dump_size || note.description_size > dump_size - description_offset) {
        return ESP_ERR_INVALID_SIZE;
    }

    const size_t stored_sha_size = min(note.description_size - sizeof(uint32_t), sha256_size - 1);
    err = esp_partition_read(partition, description_offset + sizeof(uint32_t), sha256, stored_sha_size);
    if (err != ESP_OK) {
        return err;
    }
    sha256[stored_sha_size] = '\0';

    size_t sha_length = 0;
    while (sha_length < stored_sha_size && sha256[sha_length] != '\0') {
        const char c = sha256[sha_length];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        sha_length++;
    }

    return sha_length >= 8 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t GET_system_coredump(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Core dumps can contain credentials and other sensitive task RAM.
    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_core_dump_image_check();
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_INVALID_SIZE) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No valid core dump stored");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Core dump integrity check failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_send(req, "Core dump integrity check failed", HTTPD_RESP_USE_STRLEN);
    }

    size_t dump_address = 0;
    size_t dump_size = 0;
    err = esp_core_dump_image_get(&dump_address, &dump_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to locate core dump: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to locate core dump");
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
        NULL
    );
    if (partition == NULL || dump_address != partition->address || dump_size > partition->size) {
        ESP_LOGE(TAG, "Core dump location is outside the coredump partition");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid core dump location");
    }

    char *chunk = ((rest_server_context_t *) req->user_ctx)->scratch;
    char elf_sha256[65];
    err = getCoreDumpElfSha256(partition, dump_size, chunk, SCRATCH_BUFSIZE, elf_sha256, sizeof(elf_sha256));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read core dump ELF identity: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to identify core dump ELF");
    }

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    if (httpd_resp_set_hdr(req, "X-ESP-App-ELF-SHA256", elf_sha256) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set core dump ELF identity header");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to identify core dump ELF");
    }

    size_t offset = 0;

    while (offset < dump_size) {
        const size_t chunk_size = min(dump_size - offset, (size_t) SCRATCH_BUFSIZE);
        err = esp_partition_read(partition, offset, chunk, chunk_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read core dump at offset %u: %s", (unsigned int) offset, esp_err_to_name(err));
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }

        if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send core dump at offset %u", (unsigned int) offset);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
        offset += chunk_size;
    }

    ESP_LOGI(TAG, "Sent %u-byte core dump", (unsigned int) dump_size);
    return httpd_resp_send_chunk(req, NULL, 0);
}

/* Simple handler for getting system handler */
esp_err_t GET_system_info(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

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
    const uint64_t DEFAULT_HISTORY_SPAN_MS = 3600ULL * 1000ULL;
    const uint64_t MAX_HISTORY_SPAN_MS = 3ULL * 3600ULL * 1000ULL;

    uint64_t start_timestamp = 0;
    uint64_t current_timestamp = 0;
    uint32_t history_limit = 0;
    bool history_requested = false;
    uint64_t history_span_ms = DEFAULT_HISTORY_SPAN_MS;
    char query_str[128];
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        char param[64];
        if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
            start_timestamp = strtoull(param, NULL, 10);
            if (start_timestamp) {
                history_requested = true;
            }
        }
        if (httpd_query_key_value(query_str, "limit", param, sizeof(param)) == ESP_OK) {
            history_limit = strtoul(param, NULL, 10);
            if (history_limit > 1000) {
                history_limit = 1000;
            }
        }
        if (httpd_query_key_value(query_str, "history_span", param, sizeof(param)) == ESP_OK) {
            history_span_ms = strtoull(param, NULL, 10);
            if (history_span_ms > MAX_HISTORY_SPAN_MS) {
                history_span_ms = MAX_HISTORY_SPAN_MS;
            }
            if (history_span_ms == 0) {
                history_span_ms = DEFAULT_HISTORY_SPAN_MS;
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

    bool shutdown = POWER_MANAGEMENT_MODULE.isShutdown();

    // Get configuration strings from NVS
    char *ssid               = Config::getWifiSSID();
    char *hostname           = Config::getHostname();
    char *stratumURL         = Config::getStratumURL();
    char *stratumUser        = Config::getStratumUser();
    char *fallbackStratumURL = Config::getStratumFallbackURL();
    char *fallbackStratumUser= Config::getStratumFallbackUser();

    char *sv2_auth = Config::getSV2AuthorityPubkey();
    char *fb_sv2_auth = Config::getFallbackSV2AuthorityPubkey();


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
    doc["maxVoltage"]         = board->getMaxVin();
    doc["minVoltage"]         = board->getMinVin();
    doc["current"]            = POWER_MANAGEMENT_MODULE.getCurrent();           // mA (raw)
    doc["currentA"]           = POWER_MANAGEMENT_MODULE.getCurrent() / 1000.0f; // A (UI)
    doc["minCurrentA"]        = board->getMinCurrentA(); // A
    doc["maxCurrentA"]        = board->getMaxCurrentA(); // A
    doc["temp"]               = POWER_MANAGEMENT_MODULE.getChipTempMax();
    doc["vrTemp"]             = POWER_MANAGEMENT_MODULE.getVRTemp();
    doc["vrTempInt"]          = POWER_MANAGEMENT_MODULE.getVRTempInt();
    doc["hashRateTimestamp"]  = history->getCurrentTimestamp();
    // set hashrate values to 0 in shutdown
    doc["hashRate"]           = !shutdown ? SYSTEM_MODULE.getCurrentHashrate() : 0.0;
    doc["hashRate_1m"]        = !shutdown ? history->getCurrentHashrate1m()    : 0.0;
    doc["hashRate_10m"]       = !shutdown ? history->getCurrentHashrate10m()   : 0.0;
    doc["hashRate_1h"]        = !shutdown ? history->getCurrentHashrate1h()    : 0.0;
    doc["hashRate_1d"]        = !shutdown ? history->getCurrentHashrate1d()    : 0.0;
    doc["coreVoltage"]        = board->getAsicVoltageMillis();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["coreVoltageActual"]  = (int) (board->getVout() * 1000.0f);
    doc["fanspeed"]           = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["manualFanSpeed"]     = Config::getFanSpeed();
    doc["fanrpm"]             = POWER_MANAGEMENT_MODULE.getFanRPM(0);
    doc["fanrpm2"]            = (board->getNumFans() > 1) ? POWER_MANAGEMENT_MODULE.getFanRPM(1) : 0;
    doc["fanspeed2"]          = (board->getNumFans() > 1) ? POWER_MANAGEMENT_MODULE.getFanPerc(1) : 0;
    doc["fanCount"]           = board->getNumFans();


    doc["lastpingrtt"]        = get_last_ping_rtt();
    doc["recentpingloss"]     = get_recent_ping_loss();
    doc["shutdown"]           = POWER_MANAGEMENT_MODULE.isShutdown();
    doc["duplicateHWNonces"]  = getDuplicateHWNonces();

    JsonObject stratum_obj = doc["stratum"].to<JsonObject>();

    // kept for swarm compatibility
    doc["poolDifficulty"]     = STRATUM_MANAGER->getPoolDifficulty();
    doc["networkDifficulty"]  = STRATUM_MANAGER->getNetworkDifficulty();
    doc["foundBlocks"]        = STRATUM_MANAGER->getFoundBlocks();
    doc["totalFoundBlocks"]   = STRATUM_MANAGER->getTotalFoundBlocks();
    doc["sharesAccepted"]     = STRATUM_MANAGER->getSharesAccepted();
    doc["sharesRejected"]     = STRATUM_MANAGER->getSharesRejected();
    doc["bestDiff"]           = STRATUM_MANAGER->getBestDiff();
    doc["bestSessionDiff"]    = STRATUM_MANAGER->getBestSessionDiff();

    // v1 /info: slim legacy shape (external clients like the Blocktrainer terminal)
    STRATUM_MANAGER->getManagerInfoJson(stratum_obj, false);

    // asic temps
    {
        JsonArray arr = doc["asicTemps"].to<JsonArray>();
        for (int i=0;i<board->getAsicCount();i++) {
            arr.add(board->getChipTemp(i));
        }
    }

    // If history was requested, add the history data as a nested object
    if (!shutdown && history_requested) {
        uint64_t span = history_span_ms;
        uint64_t end_timestamp = start_timestamp + span;
        JsonObject json_history = doc["history"].to<JsonObject>();

        History *history = SYSTEM_MODULE.getHistory();
        history->exportHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp, history_limit);
    }

    // settings
    PidSettings *pid = board->getPidSettings();
    doc["pidTargetTemp"]      = board->isPIDAvailable() ? pid->targetTemp : -1;
    doc["pidP"]               = (float) pid->p / 100.0f;
    doc["pidI"]               = (float) pid->i / 100.0f;
    doc["pidD"]               = (float) pid->d / 100.0f;

    // Per-channel fan settings (new API; ch0 mirrors existing flat fields for compat)
    {
        JsonArray fans = doc["fans"].to<JsonArray>();
        int numFans = board->getNumFans();
        for (int ch = 0; ch < numFans; ch++) {
            PidSettings* fanPid = board->getPidSettings(ch);
            JsonObject fan = fans.add<JsonObject>();
            fan["label"]        = board->getFanLabel(ch);
            fan["mode"]         = board->getFanMode(ch);
            fan["manualSpeed"]  = Config::getFanManualSpeed(ch);
            fan["overheatTemp"] = Config::getFanOverheatTemp(ch);
            fan["rpm"]          = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
            fan["speedPerc"]    = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
            JsonObject pid_obj  = fan["pid"].to<JsonObject>();
            pid_obj["targetTemp"] = board->isPIDAvailable() ? (int) fanPid->targetTemp : -1;
            pid_obj["p"]          = (float) fanPid->p / 100.0f;
            pid_obj["i"]          = (float) fanPid->i / 100.0f;
            pid_obj["d"]          = (float) fanPid->d / 100.0f;
        }
    }

    doc["hostname"]           = hostname;
    doc["ssid"]               = ssid;
    doc["stratumURL"]         = stratumURL;
    doc["stratumPort"]        = Config::getStratumPortNumber();
    doc["stratumUser"]        = stratumUser;
    doc["stratumEnonceSubscribe"] = Config::isStratumEnonceSubscribe();
    doc["stratumTLS"]         = Config::isStratumTLS();
    doc["fallbackStratumURL"] = fallbackStratumURL;
    doc["fallbackStratumPort"]= Config::getStratumFallbackPortNumber();
    doc["fallbackStratumUser"] = fallbackStratumUser;
    doc["fallbackStratumEnonceSubscribe"] = Config::isStratumFallbackEnonceSubscribe();
    doc["fallbackStratumTLS"] = Config::isStratumFallbackTLS();
    doc["stratumProtocol"]    = Config::getStratumProtocol();
    doc["fallbackStratumProtocol"] = Config::getFallbackStratumProtocol();
    doc["sv2AuthorityPubkey"] = sv2_auth;
    doc["fallbackSv2AuthorityPubkey"] = fb_sv2_auth;
    doc["sv2ChannelType"]     = Config::getSV2ChannelType();
    doc["fallbackSv2ChannelType"] = Config::getFallbackSV2ChannelType();
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
    doc["autofanspeed"]       = board->getFanMode(0);
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

    free(sv2_auth);
    free(fb_sv2_auth);

    return ret;
}



esp_err_t PATCH_update_settings(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

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
        Config::setFanPolarity(doc["invertfanpolarity"].as<bool>());
    }
    if (doc["autofanspeed"].is<uint16_t>()) {
        Config::setTempControlMode(doc["autofanspeed"].as<uint16_t>());
    }
    if (doc["manualFanSpeed"].is<uint16_t>()) {
        Config::setFanSpeed(doc["manualFanSpeed"].as<uint16_t>());
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

    // Per-channel fan settings: fans[0] maps to ch0 NVS keys, fans[1] to ch1 NVS keys
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
                    Config::setFanPidP(ch, (uint16_t) (p["p"].as<float>() * 100.0f));
                if (p["i"].is<float>())
                    Config::setFanPidI(ch, (uint16_t) (p["i"].as<float>() * 100.0f));
                if (p["d"].is<float>())
                    Config::setFanPidD(ch, (uint16_t) (p["d"].as<float>() * 100.0f));
            }
            ch++;
        }
    }

    // save stratum settings
    STRATUM_MANAGER->saveSettings(doc);

    doc.clear();

    Config::flush();

    // Signal the end of the response
    httpd_resp_send_chunk(req, NULL, 0);

    // Reload settings after update
    Board* board = SYSTEM_MODULE.getBoard();
    board->loadSettings();

    // Reload fan controller settings (picks up both ch0 and ch1 changes)
    POWER_MANAGEMENT_MODULE.getFanController().loadSettings();

    // reload settings of system module (and display)
    SYSTEM_MODULE.loadSettings();

    // reload settings, trigger reconnect if stratum config changed
    STRATUM_MANAGER->loadSettings();

    return ESP_OK;
}

esp_err_t GET_system_asic(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

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
    doc["absMinVoltage"]    = board->getAbsMinAsicVoltageMillis();
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

    esp_err_t ret = sendJsonResponse(req, doc);
    doc.clear();
    return ret;
}

esp_err_t POST_reset_stats(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    STRATUM_MANAGER->resetSessionStats();

    ESP_LOGI(TAG, "Session stats reset by user");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}
