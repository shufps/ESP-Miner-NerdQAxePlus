#pragma once

// clang-format off

#include <stdint.h>

// Max length 15

#define NVS_CONFIG_WIFI_SSID "wifissid"
#define NVS_CONFIG_WIFI_PASS "wifipass"
#define NVS_CONFIG_HOSTNAME "hostname"
#define NVS_CONFIG_STRATUM_URL "stratumurl"
#define NVS_CONFIG_STRATUM_PORT "stratumport"
#define NVS_CONFIG_STRATUM_USER "stratumuser"
#define NVS_CONFIG_STRATUM_PASS "stratumpass"
#define NVS_CONFIG_STRATUM_FALLBACK_URL "fbstratumurl"
#define NVS_CONFIG_STRATUM_FALLBACK_PORT "fbstratumport"
#define NVS_CONFIG_STRATUM_FALLBACK_USER "fbstratumuser"
#define NVS_CONFIG_STRATUM_FALLBACK_PASS "fbstratumpass"
#define NVS_CONFIG_STRATUM_DIFFICULTY "stratumdiff"

#define NVS_CONFIG_ASIC_FREQ "asicfrequency"
#define NVS_CONFIG_ASIC_VOLTAGE "asicvoltage"
#define NVS_CONFIG_ASIC_JOB_INTERVAL "asicjobinterval"
#define NVS_CONFIG_FLIP_SCREEN "flipscreen"
#define NVS_CONFIG_INVERT_SCREEN "invertscreen"
#define NVS_CONFIG_INVERT_FAN_POLARITY "invertfanpol"
#define NVS_CONFIG_AUTO_FAN_POLARITY "autofanpol"
#define NVS_CONFIG_AUTO_FAN_SPEED "autofanspeed"
#define NVS_CONFIG_FAN_SPEED "fanspeed"
#define NVS_CONFIG_BEST_DIFF "bestdiff"
#define NVS_CONFIG_SELF_TEST "selftest"
#define NVS_CONFIG_AUTO_SCREEN_OFF "autoscreenoff"
#define NVS_CONFIG_OVERHEAT_TEMP "overheat_temp"

#define NVS_CONFIG_INFLUX_ENABLE "influx_enable"
#define NVS_CONFIG_INFLUX_URL "influx_url"
#define NVS_CONFIG_INFLUX_TOKEN "influx_token"
#define NVS_CONFIG_INFLUX_PORT "influx_port"
#define NVS_CONFIG_INFLUX_BUCKET "influx_bucket"
#define NVS_CONFIG_INFLUX_ORG "influx_org"
#define NVS_CONFIG_INFLUX_PREFIX "influx_prefix"

#define NVS_CONFIG_PID_TARGET_TEMP "pid_temp"
#define NVS_CONFIG_PID_P "pid_p"
#define NVS_CONFIG_PID_I "pid_i"
#define NVS_CONFIG_PID_D "pid_d"

#define NVS_CONFIG_ALERT_DISCORD_ENABLE "alrt_disc_en"
#define NVS_CONFIG_ALERT_DISCORD_URL    "alrt_disc_url"

#define NVS_CONFIG_SWARM "swarmconfig"

#if defined(CONFIG_FAN_MODE_MANUAL)
#define CONFIG_AUTO_FAN_SPEED_VALUE 0
#elif defined(CONFIG_FAN_MODE_CLASSIC)
#define CONFIG_AUTO_FAN_SPEED_VALUE 1
#elif defined(CONFIG_FAN_MODE_PID)
#define CONFIG_AUTO_FAN_SPEED_VALUE 2
#endif

#include <stdint.h>

namespace Config {
    char* nvs_config_get_string(const char* key, const char* default_value);
    void nvs_config_set_string(const char* key, const char* value);
    uint16_t nvs_config_get_u16(const char* key, uint16_t default_value);
    void nvs_config_set_u16(const char* key, uint16_t value);
    uint64_t nvs_config_get_u64(const char* key, uint64_t default_value);
    void nvs_config_set_u64(const char* key, uint64_t value);

    // ---- String Getters ----
    inline char* getWifiSSID() { return nvs_config_get_string(NVS_CONFIG_WIFI_SSID, CONFIG_ESP_WIFI_SSID); }
    inline char* getWifiPass() { return nvs_config_get_string(NVS_CONFIG_WIFI_PASS, CONFIG_ESP_WIFI_PASSWORD); }
    inline char* getHostname() { return nvs_config_get_string(NVS_CONFIG_HOSTNAME, CONFIG_LWIP_LOCAL_HOSTNAME); }
    inline char* getStratumURL() { return nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL); }
    inline char* getStratumUser() { return nvs_config_get_string(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER); }
    inline char* getStratumPass() { return nvs_config_get_string(NVS_CONFIG_STRATUM_PASS, CONFIG_STRATUM_PW); }
    inline char* getStratumFallbackURL() { return nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_URL, CONFIG_STRATUM_FALLBACK_URL); }
    inline char* getStratumFallbackUser() { return nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_USER, CONFIG_STRATUM_FALLBACK_USER); }
    inline char* getStratumFallbackPass() { return nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_PASS, CONFIG_STRATUM_FALLBACK_PW); }
    inline char* getInfluxURL() { return nvs_config_get_string(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL); }
    inline char* getInfluxToken() { return nvs_config_get_string(NVS_CONFIG_INFLUX_TOKEN, CONFIG_INFLUX_TOKEN); }
    inline char* getInfluxBucket() { return nvs_config_get_string(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET); }
    inline char* getInfluxOrg() { return nvs_config_get_string(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG); }
    inline char* getInfluxPrefix() { return nvs_config_get_string(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX); }
    inline char* getSwarmConfig() { return nvs_config_get_string(NVS_CONFIG_SWARM, ""); }
    inline char* getDiscordWebhook() { return nvs_config_get_string(NVS_CONFIG_ALERT_DISCORD_URL, CONFIG_ALERT_DISCORD_URL); }

    // ---- String Setters ----
    inline void setWifiSSID(const char* value) { nvs_config_set_string(NVS_CONFIG_WIFI_SSID, value); }
    inline void setWifiPass(const char* value) { nvs_config_set_string(NVS_CONFIG_WIFI_PASS, value); }
    inline void setHostname(const char* value) { nvs_config_set_string(NVS_CONFIG_HOSTNAME, value); }
    inline void setStratumURL(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_URL, value); }
    inline void setStratumUser(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_USER, value); }
    inline void setStratumPass(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_PASS, value); }
    inline void setStratumFallbackURL(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_URL, value); }
    inline void setStratumFallbackUser(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_USER, value); }
    inline void setStratumFallbackPass(const char* value) { nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_PASS, value); }
    inline void setInfluxURL(const char* value) { nvs_config_set_string(NVS_CONFIG_INFLUX_URL, value); }
    inline void setInfluxToken(const char* value) { nvs_config_set_string(NVS_CONFIG_INFLUX_TOKEN, value); }
    inline void setInfluxBucket(const char* value) { nvs_config_set_string(NVS_CONFIG_INFLUX_BUCKET, value); }
    inline void setInfluxOrg(const char* value) { nvs_config_set_string(NVS_CONFIG_INFLUX_ORG, value); }
    inline void setInfluxPrefix(const char* value) { nvs_config_set_string(NVS_CONFIG_INFLUX_PREFIX, value); }
    inline void setSwarmConfig(const char* value) { nvs_config_set_string(NVS_CONFIG_SWARM, value); }
    inline void setDiscordWebhook(const char* value) { nvs_config_set_string(NVS_CONFIG_ALERT_DISCORD_URL, value); }

    // ---- uint16_t Getters ----
    inline uint16_t getStratumPortNumber() { return nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT); }
    inline uint16_t getStratumFallbackPortNumber() { return nvs_config_get_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, CONFIG_STRATUM_FALLBACK_PORT); }
    inline uint16_t getFanSpeed() { return nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, CONFIG_FAN_SPEED); }
    inline uint16_t getOverheatTemp() { return nvs_config_get_u16(NVS_CONFIG_OVERHEAT_TEMP, CONFIG_OVERHEAT_TEMP); }
    inline uint16_t getInfluxPort() { return nvs_config_get_u16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT); }
    inline uint16_t getTempControlMode() { return nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, CONFIG_AUTO_FAN_SPEED_VALUE); }


    // ---- uint16_t Setters ----
    inline void setAsicFrequency(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, value); }
    inline void setAsicVoltage(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, value); }
    inline void setAsicJobInterval(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_ASIC_JOB_INTERVAL, value); }
    inline void setStratumPortNumber(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_STRATUM_PORT, value); }
    inline void setStratumFallbackPortNumber(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, value); }
    inline void setFanSpeed(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, value); }
    inline void setOverheatTemp(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_OVERHEAT_TEMP, value); }
    inline void setInfluxPort(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_INFLUX_PORT, value); }
    inline void setTempControlMode(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, value); }

    inline void setPidTargetTemp(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_PID_TARGET_TEMP, value); }
    inline void setPidP(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_PID_P, value); }
    inline void setPidI(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_PID_I, value); }
    inline void setPidD(uint16_t value) { nvs_config_set_u16(NVS_CONFIG_PID_D, value); }

    // ---- uint64_t Getters ----
    inline uint64_t getBestDiff() { return nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0); }
    inline uint32_t getStratumDifficulty() { return (uint32_t) nvs_config_get_u64(NVS_CONFIG_STRATUM_DIFFICULTY, CONFIG_STRATUM_DIFFICULTY); }

    // ---- uint64_t Setters ----
    inline void setBestDiff(uint64_t value) { nvs_config_set_u64(NVS_CONFIG_BEST_DIFF, value); }
    inline void setStratumDifficulty(uint32_t value) { nvs_config_set_u64(NVS_CONFIG_STRATUM_DIFFICULTY, value); }

    // ---- Boolean Getters (Stored as uint16_t but used as bool) ----
    inline bool isInvertScreenEnabled() { return nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0) != 0; } // todo unused?
    inline bool isSelfTestEnabled() { return nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 0) != 0; }
    inline bool isAutoScreenOffEnabled() { return nvs_config_get_u16(NVS_CONFIG_AUTO_SCREEN_OFF, CONFIG_AUTO_SCREEN_OFF_VALUE) != 0; }
    inline bool isInfluxEnabled() { return nvs_config_get_u16(NVS_CONFIG_INFLUX_ENABLE, CONFIG_INFLUX_ENABLE_VALUE) != 0; }
    inline bool isDiscordAlertEnabled() { return nvs_config_get_u16(NVS_CONFIG_ALERT_DISCORD_ENABLE, CONFIG_ALERT_DISCORD_ENABLE_VALUE) != 0; }

    // ---- Boolean Setters ----
    inline void setFlipScreen(bool value) { nvs_config_set_u16(NVS_CONFIG_FLIP_SCREEN, value ? 1 : 0); }
    inline void setInvertScreen(bool value) { nvs_config_set_u16(NVS_CONFIG_INVERT_SCREEN, value ? 1 : 0); }
    inline void setInvertFanPolarity(bool value) { nvs_config_set_u16(NVS_CONFIG_INVERT_FAN_POLARITY, value ? 1 : 0); }
    inline void setAutoFanPolarity(bool value) { nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_POLARITY, value ? 1 : 0); }
    inline void setSelfTest(bool value) { nvs_config_set_u16(NVS_CONFIG_SELF_TEST, value ? 1 : 0); }
    inline void setAutoScreenOff(bool value) { nvs_config_set_u16(NVS_CONFIG_AUTO_SCREEN_OFF, value ? 1 : 0); }
    inline void setInfluxEnabled(bool value) { nvs_config_set_u16(NVS_CONFIG_INFLUX_ENABLE, value ? 1 : 0); }
    inline void setDiscordAlertEnabled(bool value) { nvs_config_set_u16(NVS_CONFIG_ALERT_DISCORD_ENABLE, value ? 1 : 0); }


    // with board specific default values
    inline uint16_t getAsicFrequency(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, d); }
    inline uint16_t getAsicVoltage(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, d); }
    inline uint16_t getAsicJobInterval(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_ASIC_JOB_INTERVAL, d); }
    inline bool isFlipScreenEnabled(bool d) { return nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, d ? 1 : 0) != 0; }
    inline bool isInvertFanPolarityEnabled(bool d) { return nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, d ? 1 : 0) != 0; }
    inline bool isAutoFanPolarityEnabled(bool d) { return nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_POLARITY, d ? 1 : 0) != 0; }
    inline uint16_t getPidTargetTemp(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_PID_TARGET_TEMP, d); }
    inline uint16_t getPidP(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_PID_P, d); }
    inline uint16_t getPidI(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_PID_I, d); }
    inline uint16_t getPidD(uint16_t d) { return nvs_config_get_u16(NVS_CONFIG_PID_D, d); }
}
