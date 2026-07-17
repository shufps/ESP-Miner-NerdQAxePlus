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
#define NVS_CONFIG_STRATUM_ENONCE_SUB "stratumesub"
#define NVS_CONFIG_STRATUM_TLS "stratumtls"
#define NVS_CONFIG_STRATUM_FALLBACK_URL "fbstratumurl"
#define NVS_CONFIG_STRATUM_FALLBACK_PORT "fbstratumport"
#define NVS_CONFIG_STRATUM_FALLBACK_USER "fbstratumuser"
#define NVS_CONFIG_STRATUM_FALLBACK_PASS "fbstratumpass"
#define NVS_CONFIG_STRATUM_FALLBACK_ENONCE_SUB "fbstratumesub"
#define NVS_CONFIG_STRATUM_FALLBACK_TLS "fbstratumtls"
#define NVS_CONFIG_STRATUM_DIFFICULTY "stratumdiff"
#define NVS_CONFIG_STRATUM_KEEPALIVE "stratum_keep"

#define NVS_CONFIG_ASIC_FREQ "asicfrequency"
#define NVS_CONFIG_ASIC_VOLTAGE "asicvoltage"
#define NVS_CONFIG_ASIC_JOB_INTERVAL "asicjobinterval"
#define NVS_CONFIG_FLIP_SCREEN "flipscreen"
#define NVS_CONFIG_INVERT_SCREEN "invertscreen"
#define NVS_CONFIG_INVERT_FAN_POLARITY "invertfanpol"   // kept for downgrade compatibility
#define NVS_CONFIG_AUTO_FAN_POLARITY "autofanpol"       // kept for downgrade compatibility
#define NVS_CONFIG_FAN_PWM_POLARITY "pwmfanpol"         // new setting
#define NVS_CONFIG_AUTO_FAN_SPEED "autofanspeed"
#define NVS_CONFIG_FAN_SPEED "fanspeed"
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

// Fan channel 1 (independent second fan, e.g. for VR temp)
#define NVS_CONFIG_FAN1_SPEED    "fan1speed"
#define NVS_CONFIG_FAN1_MODE     "fan1mode"
#define NVS_CONFIG_FAN1_PID_TEMP "fan1_pid_temp"
#define NVS_CONFIG_FAN1_PID_P    "fan1_pid_p"
#define NVS_CONFIG_FAN1_PID_I    "fan1_pid_i"
#define NVS_CONFIG_FAN1_PID_D    "fan1_pid_d"
#define NVS_CONFIG_FAN1_OVERHEAT "fan1_overheat"
#define NVS_CONFIG_FAN_PID_USE_MAX "fan_pid_max"

#define NVS_CONFIG_MEMPOOL_URL "mempool_url"
#define NVS_CONFIG_MEMPOOL_CUSTOM "mempool_cust"

#define CONFIG_DONATE_ADDR "bc1q7n70rumyv6lvu8avpml0c3uggvssfu52egum3q"

#define NVS_CONFIG_ALERT_DISCORD_WATCHDOG_ENABLE "alrt_disc_en"
#define NVS_CONFIG_ALERT_DISCORD_URL    "alrt_disc_url"
#define NVS_CONFIG_ALERT_DISCORD_BLOCK_FOUND_ENABLE "alrt_disc_bf_en"
#define NVS_CONFIG_ALERT_DISCORD_BEST_DIFF "alrt_disc_bd_en"
#define NVS_CONFIG_ALERT_DISCORD_COINBASE_VERIFY "alrt_disc_cb_en"

#define NVS_CONFIG_SHOW_BLOCK_FOUND_ENABLE "block_found_en"

#define NVS_CONFIG_SWARM "swarmconfig"

#define NVS_CONFIG_CAN_ENABLED "can_enabled"

#define NVS_CONFIG_VR_FREQUENCY "vr_frequency"

// device global stats
#define NVS_TOTAL_FOUND_BLOCKS "totalblocks"
#define NVS_CONFIG_BEST_DIFF "bestdiff"


// Coinbase verification (per pool)
#define NVS_CONFIG_COINBASE_VERIFY_MODE    "cb_verify_mode"
#define NVS_CONFIG_COINBASE_MAX_FEE        "cb_max_fee"
#define NVS_CONFIG_COINBASE_VERIFY_FORCE   "cb_vfy_force"
#define NVS_CONFIG_FB_COINBASE_VERIFY_MODE "fb_cb_vfy_mode"
#define NVS_CONFIG_FB_COINBASE_MAX_FEE     "fb_cb_max_fee"
#define NVS_CONFIG_FB_COINBASE_VERIFY_FORCE "fb_cb_vfy_frc"

// OTP
#define NVS_CONFIG_OTP_SECRET "otp_secret"
#define NVS_CONFIG_OTP_ENABLED "otp_enabled"
#define NVS_CONFIG_OTP_LAST_STEP "otp_last_step"
#define NVS_CONFIG_OTP_USED_MASK "otp_used_mask"
#define NVS_CONFIG_OTP_SESSION_KEY "otp_sess_key"
#define NVS_CONFIG_OTP_BOOT_ID "otp_boot_id"

#define NVS_CONFIG_POOL_MODE_BALANCE "pool_balance"
#define NVS_CONFIG_POOL_MODE "pool_mode"

// Stratum V2
#define NVS_CONFIG_STRATUM_PROTOCOL "sv2_proto"
#define NVS_CONFIG_SV2_AUTHORITY_PUBKEY "sv2_auth_pk"
#define NVS_CONFIG_SV2_CHANNEL_TYPE "sv2_chan_type"
#define NVS_CONFIG_FB_STRATUM_PROTOCOL "fbsv2_proto"
#define NVS_CONFIG_FB_SV2_AUTHORITY_PUBKEY "fbsv2_authpk"
#define NVS_CONFIG_FB_SV2_CHANNEL_TYPE "fbsv2_chtype"

#if defined(CONFIG_FAN_MODE_MANUAL)
#define CONFIG_AUTO_FAN_SPEED_VALUE 0
#elif defined(CONFIG_FAN_MODE_CLASSIC)
#define CONFIG_AUTO_FAN_SPEED_VALUE 1
#elif defined(CONFIG_FAN_MODE_PID)
#define CONFIG_AUTO_FAN_SPEED_VALUE 2
#endif

#include <stdint.h>

namespace Config {
    // ---- Lifecycle ----
    void init();     // Load flat JSON blob or migrate legacy keys. Call once at boot.
    void flush();    // Signal the non-PSRAM flush task to persist batched changes.

    // ---- Direct document access (for API handlers — include ArduinoJson.h first) ----
    // JsonDocument& getDoc();
    void cfgLock();
    void cfgUnlock();

    // ---- Generic typed accessors (backed by the in-memory flat JSON doc) ----
    char* cfgGetStrAlloc(const char* path, const char* def = "");  // caller must free
    void  cfgSetStr(const char* path, const char* value);
    uint16_t cfgGetU16(const char* path, uint16_t def = 0);
    void     cfgSetU16(const char* path, uint16_t value);
    bool     cfgGetBool(const char* path, bool def = false);
    void     cfgSetBool(const char* path, bool value);
    uint64_t cfgGetU64(const char* path, uint64_t def = 0);
    void     cfgSetU64(const char* path, uint64_t value);

    bool     cfgHasKey(const char* path);

    // ---- String Getters ----
    inline char* getWifiSSID() { return cfgGetStrAlloc(NVS_CONFIG_WIFI_SSID, CONFIG_ESP_WIFI_SSID); }
    inline char* getWifiPass() { return cfgGetStrAlloc(NVS_CONFIG_WIFI_PASS, CONFIG_ESP_WIFI_PASSWORD); }
    inline char* getHostname() { return cfgGetStrAlloc(NVS_CONFIG_HOSTNAME, CONFIG_LWIP_LOCAL_HOSTNAME); }
    inline char* getStratumURL() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL); }
    inline char* getStratumUser() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER); }
    inline char* getStratumPass() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_PASS, CONFIG_STRATUM_PW); }
    inline char* getStratumFallbackURL() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_FALLBACK_URL, CONFIG_STRATUM_FALLBACK_URL); }
    inline char* getStratumFallbackUser() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_FALLBACK_USER, CONFIG_STRATUM_FALLBACK_USER); }
    inline char* getStratumFallbackPass() { return cfgGetStrAlloc(NVS_CONFIG_STRATUM_FALLBACK_PASS, CONFIG_STRATUM_FALLBACK_PW); }
    inline char* getInfluxURL() { return cfgGetStrAlloc(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL); }
    inline char* getInfluxToken() { return cfgGetStrAlloc(NVS_CONFIG_INFLUX_TOKEN, CONFIG_INFLUX_TOKEN); }
    inline char* getInfluxBucket() { return cfgGetStrAlloc(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET); }
    inline char* getInfluxOrg() { return cfgGetStrAlloc(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG); }
    inline char* getInfluxPrefix() { return cfgGetStrAlloc(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX); }
    inline char* getSwarmConfig() { return cfgGetStrAlloc(NVS_CONFIG_SWARM, ""); }
    inline char* getDiscordWebhook() { return cfgGetStrAlloc(NVS_CONFIG_ALERT_DISCORD_URL, CONFIG_ALERT_DISCORD_URL); }
    inline char* getMempoolUrl() { return cfgGetStrAlloc(NVS_CONFIG_MEMPOOL_URL, CONFIG_MEMPOOL_URL); }
    inline void setMempoolUrl(const char* value) { cfgSetStr(NVS_CONFIG_MEMPOOL_URL, value); }
    inline bool isMempoolCustom() { return cfgGetU16(NVS_CONFIG_MEMPOOL_CUSTOM, 0) != 0; }
    inline void setMempoolCustom(bool v) { cfgSetU16(NVS_CONFIG_MEMPOOL_CUSTOM, v ? 1 : 0); }

    // ---- String Setters ----
    inline void setWifiSSID(const char* value) { cfgSetStr(NVS_CONFIG_WIFI_SSID, value); }
    inline void setWifiPass(const char* value) { cfgSetStr(NVS_CONFIG_WIFI_PASS, value); }
    inline void setHostname(const char* value) { cfgSetStr(NVS_CONFIG_HOSTNAME, value); }
    inline void setStratumURL(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_URL, value); }
    inline void setStratumUser(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_USER, value); }
    inline void setStratumPass(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_PASS, value); }
    inline void setStratumFallbackURL(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_FALLBACK_URL, value); }
    inline void setStratumFallbackUser(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_FALLBACK_USER, value); }
    inline void setStratumFallbackPass(const char* value) { cfgSetStr(NVS_CONFIG_STRATUM_FALLBACK_PASS, value); }
    inline void setInfluxURL(const char* value) { cfgSetStr(NVS_CONFIG_INFLUX_URL, value); }
    inline void setInfluxToken(const char* value) { cfgSetStr(NVS_CONFIG_INFLUX_TOKEN, value); }
    inline void setInfluxBucket(const char* value) { cfgSetStr(NVS_CONFIG_INFLUX_BUCKET, value); }
    inline void setInfluxOrg(const char* value) { cfgSetStr(NVS_CONFIG_INFLUX_ORG, value); }
    inline void setInfluxPrefix(const char* value) { cfgSetStr(NVS_CONFIG_INFLUX_PREFIX, value); }
    inline void setSwarmConfig(const char* value) { cfgSetStr(NVS_CONFIG_SWARM, value); }
    inline void setDiscordWebhook(const char* value) { cfgSetStr(NVS_CONFIG_ALERT_DISCORD_URL, value); }

    // ---- uint16_t Getters ----
    inline uint16_t getStratumPortNumber() { return cfgGetU16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT); }
    inline uint16_t getStratumFallbackPortNumber() { return cfgGetU16(NVS_CONFIG_STRATUM_FALLBACK_PORT, CONFIG_STRATUM_FALLBACK_PORT); }
    inline uint16_t getFanSpeed() { return cfgGetU16(NVS_CONFIG_FAN_SPEED, CONFIG_FAN_SPEED); }
    inline uint16_t getOverheatTemp() { return cfgGetU16(NVS_CONFIG_OVERHEAT_TEMP, CONFIG_OVERHEAT_TEMP); }
    inline uint16_t getInfluxPort() { return cfgGetU16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT); }
    inline uint16_t getPoolMode() { return cfgGetU16(NVS_CONFIG_POOL_MODE, 0); }
    inline uint16_t getPoolBalance() { return cfgGetU16(NVS_CONFIG_POOL_MODE_BALANCE, 50); }

    // ---- uint16_t Setters ----
    inline void setAsicFrequency(uint16_t value) { cfgSetU16(NVS_CONFIG_ASIC_FREQ, value); }
    inline void setAsicVoltage(uint16_t value) { cfgSetU16(NVS_CONFIG_ASIC_VOLTAGE, value); }
    inline void setAsicJobInterval(uint16_t value) { cfgSetU16(NVS_CONFIG_ASIC_JOB_INTERVAL, value); }
    inline void setStratumPortNumber(uint16_t value) { cfgSetU16(NVS_CONFIG_STRATUM_PORT, value); }
    inline void setStratumFallbackPortNumber(uint16_t value) { cfgSetU16(NVS_CONFIG_STRATUM_FALLBACK_PORT, value); }
    inline void setFanSpeed(uint16_t value) { cfgSetU16(NVS_CONFIG_FAN_SPEED, value); }
    inline void setOverheatTemp(uint16_t value) { cfgSetU16(NVS_CONFIG_OVERHEAT_TEMP, value); }
    inline void setInfluxPort(uint16_t value) { cfgSetU16(NVS_CONFIG_INFLUX_PORT, value); }
    inline void setTempControlMode(uint16_t value) { cfgSetU16(NVS_CONFIG_AUTO_FAN_SPEED, value); }
    inline void setPoolMode(uint16_t value) { cfgSetU16(NVS_CONFIG_POOL_MODE, value); }
    inline void setPoolBalance(uint16_t value) { cfgSetU16(NVS_CONFIG_POOL_MODE_BALANCE, value); }

    inline void setPidTargetTemp(uint16_t value) { cfgSetU16(NVS_CONFIG_PID_TARGET_TEMP, value); }
    inline void setPidP(uint16_t value) { cfgSetU16(NVS_CONFIG_PID_P, value); }
    inline void setPidI(uint16_t value) { cfgSetU16(NVS_CONFIG_PID_I, value); }
    inline void setPidD(uint16_t value) { cfgSetU16(NVS_CONFIG_PID_D, value); }

    // Indexed fan-channel getters (ch=0 → ch0 NVS keys, ch=1 → fan1 NVS keys)
    // ch0 defaults: mode=CONFIG_AUTO_FAN_SPEED_VALUE, speed=CONFIG_FAN_SPEED, overheat=CONFIG_OVERHEAT_TEMP
    // ch1 defaults: mode=3 (linked), speed=100%, overheat=80°C
    inline uint16_t getFanMode(int ch, uint16_t def) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_AUTO_FAN_SPEED, def)
                       : cfgGetU16(NVS_CONFIG_FAN1_MODE, def);
    }
    inline uint16_t getFanManualSpeed(int ch) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_FAN_SPEED, CONFIG_FAN_SPEED)
                       : cfgGetU16(NVS_CONFIG_FAN1_SPEED, 100);
    }
    inline uint16_t getFanOverheatTemp(int ch) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_OVERHEAT_TEMP, CONFIG_OVERHEAT_TEMP)
                       : cfgGetU16(NVS_CONFIG_FAN1_OVERHEAT, CONFIG_OVERHEAT_TEMP);
    }
    inline uint16_t getFanPidTargetTemp(int ch, uint16_t d) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_PID_TARGET_TEMP, d)
                       : cfgGetU16(NVS_CONFIG_FAN1_PID_TEMP, d);
    }
    inline uint16_t getFanPidP(int ch, uint16_t d) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_PID_P, d)
                       : cfgGetU16(NVS_CONFIG_FAN1_PID_P, d);
    }
    inline uint16_t getFanPidI(int ch, uint16_t d) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_PID_I, d)
                       : cfgGetU16(NVS_CONFIG_FAN1_PID_I, d);
    }
    inline uint16_t getFanPidD(int ch, uint16_t d) {
        return ch == 0 ? cfgGetU16(NVS_CONFIG_PID_D, d)
                       : cfgGetU16(NVS_CONFIG_FAN1_PID_D, d);
    }

    // Indexed fan-channel setters
    inline void setFanMode(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_AUTO_FAN_SPEED, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_MODE, v);
    }
    inline void setFanManualSpeed(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_FAN_SPEED, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_SPEED, v);
    }
    inline void setFanOverheatTemp(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_OVERHEAT_TEMP, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_OVERHEAT, v);
    }
    inline void setFanPidTargetTemp(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_PID_TARGET_TEMP, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_PID_TEMP, v);
    }
    inline void setFanPidP(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_PID_P, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_PID_P, v);
    }
    inline void setFanPidI(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_PID_I, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_PID_I, v);
    }
    inline void setFanPidD(int ch, uint16_t v) {
        if (ch == 0) cfgSetU16(NVS_CONFIG_PID_D, v);
        else         cfgSetU16(NVS_CONFIG_FAN1_PID_D, v);
    }

    // Fan PID: use max(ASIC, VReg) as input instead of ASIC only
    inline bool isFanPidUseMax() { return cfgGetU16(NVS_CONFIG_FAN_PID_USE_MAX, 1) != 0; }
    inline void setFanPidUseMax(bool v) { cfgSetU16(NVS_CONFIG_FAN_PID_USE_MAX, v ? 1 : 0); }

    // ---- uint64_t Getters ----
    uint64_t getBestDiff();
    uint32_t getTotalFoundBlocks();
    inline uint32_t getStratumDifficulty() { return (uint32_t) cfgGetU64(NVS_CONFIG_STRATUM_DIFFICULTY, CONFIG_STRATUM_DIFFICULTY); }

    // ---- uint64_t Setters ----
    void setBestDiff(uint64_t value);
    void setTotalFoundBlocks(uint32_t value);
    inline void setStratumDifficulty(uint32_t value) { cfgSetU64(NVS_CONFIG_STRATUM_DIFFICULTY, value); }
    inline void setVrFrequency(uint32_t value) { cfgSetU64(NVS_CONFIG_VR_FREQUENCY, value); }

    // ---- Boolean Getters (Stored as uint16_t but used as bool) ----
    inline bool isInvertScreenEnabled() { return cfgGetU16(NVS_CONFIG_INVERT_SCREEN, 0) != 0; } // todo unused?
    inline bool isSelfTestEnabled() { return cfgGetU16(NVS_CONFIG_SELF_TEST, 0) != 0; }
    inline bool isAutoScreenOffEnabled() { return cfgGetU16(NVS_CONFIG_AUTO_SCREEN_OFF, CONFIG_AUTO_SCREEN_OFF_VALUE) != 0; }
    inline bool isInfluxEnabled() { return cfgGetU16(NVS_CONFIG_INFLUX_ENABLE, CONFIG_INFLUX_ENABLE_VALUE) != 0; }
    inline bool isDiscordWatchdogAlertEnabled() { return cfgGetU16(NVS_CONFIG_ALERT_DISCORD_WATCHDOG_ENABLE, CONFIG_ALERT_DISCORD_WATCHDOG_ENABLE_VALUE) != 0; }
    inline bool isDiscordBlockFoundAlertEnabled() { return cfgGetU16(NVS_CONFIG_ALERT_DISCORD_BLOCK_FOUND_ENABLE, CONFIG_ALERT_DISCORD_BLOCK_FOUND_ENABLE_VALUE) != 0; }
    inline bool isDiscordBestDiffAlertEnabled() { return cfgGetU16(NVS_CONFIG_ALERT_DISCORD_BEST_DIFF, CONFIG_ALERT_DISCORD_BEST_DIFF_ENABLE_VALUE) != 0; }
    inline bool isDiscordCoinbaseVerifyAlertEnabled() { return cfgGetU16(NVS_CONFIG_ALERT_DISCORD_COINBASE_VERIFY, 0) != 0; }
    inline bool isStratumKeepaliveEnabled() { return cfgGetU16(NVS_CONFIG_STRATUM_KEEPALIVE, CONFIG_STRATUM_KEEPALIVE_ENABLE_VALUE) != 0; }
    inline bool isStratumEnonceSubscribe() { return cfgGetU16(NVS_CONFIG_STRATUM_ENONCE_SUB, CONFIG_STRATUM_ENONCE_SUBSCRIBE_VALUE) != 0; }
    inline bool isStratumFallbackEnonceSubscribe() { return cfgGetU16(NVS_CONFIG_STRATUM_FALLBACK_ENONCE_SUB, CONFIG_STRATUM_FALLBACK_ENONCE_SUBSCRIBE_VALUE) != 0; }
    inline bool isStratumTLS() { return cfgGetU16(NVS_CONFIG_STRATUM_TLS, CONFIG_STRATUM_TLS_VALUE) != 0; }
    inline bool isStratumFallbackTLS() { return cfgGetU16(NVS_CONFIG_STRATUM_FALLBACK_TLS, CONFIG_STRATUM_FALLBACK_TLS_VALUE) != 0; }
    inline bool isShowBlockFoundEnabled() { return cfgGetU16(NVS_CONFIG_SHOW_BLOCK_FOUND_ENABLE, CONFIG_SHOW_BLOCK_FOUND_ENABLE_VALUE) != 0; }
    inline bool isCanEnabled() { return cfgGetU16(NVS_CONFIG_CAN_ENABLED, 0) != 0; }

    // Stratum V2
    inline uint16_t getStratumProtocol() { return cfgGetU16(NVS_CONFIG_STRATUM_PROTOCOL, 0); }
    inline void setStratumProtocol(uint16_t value) { cfgSetU16(NVS_CONFIG_STRATUM_PROTOCOL, value); }
    inline uint16_t getFallbackStratumProtocol() { return cfgGetU16(NVS_CONFIG_FB_STRATUM_PROTOCOL, 0); }
    inline void setFallbackStratumProtocol(uint16_t value) { cfgSetU16(NVS_CONFIG_FB_STRATUM_PROTOCOL, value); }
    inline char* getSV2AuthorityPubkey() { return cfgGetStrAlloc(NVS_CONFIG_SV2_AUTHORITY_PUBKEY, ""); }
    inline void setSV2AuthorityPubkey(const char* value) { cfgSetStr(NVS_CONFIG_SV2_AUTHORITY_PUBKEY, value); }
    inline char* getFallbackSV2AuthorityPubkey() { return cfgGetStrAlloc(NVS_CONFIG_FB_SV2_AUTHORITY_PUBKEY, ""); }
    inline void setFallbackSV2AuthorityPubkey(const char* value) { cfgSetStr(NVS_CONFIG_FB_SV2_AUTHORITY_PUBKEY, value); }
    inline uint16_t getSV2ChannelType() { return cfgGetU16(NVS_CONFIG_SV2_CHANNEL_TYPE, 0); }
    inline void setSV2ChannelType(uint16_t value) { cfgSetU16(NVS_CONFIG_SV2_CHANNEL_TYPE, value); }
    inline uint16_t getFallbackSV2ChannelType() { return cfgGetU16(NVS_CONFIG_FB_SV2_CHANNEL_TYPE, 0); }
    inline void setFallbackSV2ChannelType(uint16_t value) { cfgSetU16(NVS_CONFIG_FB_SV2_CHANNEL_TYPE, value); }

    // ---- Boolean Setters ----
    inline void setFlipScreen(bool value) { cfgSetU16(NVS_CONFIG_FLIP_SCREEN, value ? 1 : 0); }
    inline void setInvertScreen(bool value) { cfgSetU16(NVS_CONFIG_INVERT_SCREEN, value ? 1 : 0); }
    inline void setFanPolarity(bool value) { cfgSetU16(NVS_CONFIG_FAN_PWM_POLARITY, value ? 1 : 0); }
    inline void setSelfTest(bool value) { cfgSetU16(NVS_CONFIG_SELF_TEST, value ? 1 : 0); }
    inline void setAutoScreenOff(bool value) { cfgSetU16(NVS_CONFIG_AUTO_SCREEN_OFF, value ? 1 : 0); }
    inline void setInfluxEnabled(bool value) { cfgSetU16(NVS_CONFIG_INFLUX_ENABLE, value ? 1 : 0); }
    inline void setDiscordWatchdogAlertEnabled(bool value) { cfgSetU16(NVS_CONFIG_ALERT_DISCORD_WATCHDOG_ENABLE, value ? 1 : 0); }
    inline void setDiscordAlertBlockFoundEnabled(bool value) { cfgSetU16(NVS_CONFIG_ALERT_DISCORD_BLOCK_FOUND_ENABLE, value ? 1 : 0); }
    inline void setDiscordAlertBestDiffEnabled(bool value) { cfgSetU16(NVS_CONFIG_ALERT_DISCORD_BEST_DIFF, value ? 1 : 0); }
    inline void setDiscordCoinbaseVerifyAlertEnabled(bool value) { cfgSetU16(NVS_CONFIG_ALERT_DISCORD_COINBASE_VERIFY, value ? 1 : 0); }
    inline void setStratumKeepaliveEnabled(bool value) { cfgSetU16(NVS_CONFIG_STRATUM_KEEPALIVE, value ? 1 : 0); }
    inline void setStratumEnonceSubscribe(bool value) { cfgSetU16(NVS_CONFIG_STRATUM_ENONCE_SUB, value ? 1 : 0); }
    inline void setStratumFallbackEnonceSubscribe(bool value) { cfgSetU16(NVS_CONFIG_STRATUM_FALLBACK_ENONCE_SUB, value ? 1 : 0); }
    inline void setStratumTLS(bool value) { cfgSetU16(NVS_CONFIG_STRATUM_TLS, value ? 1 : 0); }
    inline void setStratumFallbackTLS(bool value) { cfgSetU16(NVS_CONFIG_STRATUM_FALLBACK_TLS, value ? 1 : 0); }
    inline void setShowBlockFoundEnabled(bool value) { cfgSetU16(NVS_CONFIG_SHOW_BLOCK_FOUND_ENABLE, value ? 1 : 0); }
    inline void setCanEnabled(bool value) { cfgSetU16(NVS_CONFIG_CAN_ENABLED, value ? 1 : 0); }

    // with board specific default values
    inline uint16_t getAsicFrequency(uint16_t d) { return cfgGetU16(NVS_CONFIG_ASIC_FREQ, d); }
    inline uint16_t getAsicVoltage(uint16_t d) { return cfgGetU16(NVS_CONFIG_ASIC_VOLTAGE, d); }
    inline uint16_t getAsicJobInterval(uint16_t d) { return cfgGetU16(NVS_CONFIG_ASIC_JOB_INTERVAL, d); }
    inline bool isFlipScreenEnabled(bool d) { return cfgGetU16(NVS_CONFIG_FLIP_SCREEN, d ? 1 : 0) != 0; }
    inline bool isFanPolarity(bool d) { return cfgGetU16(NVS_CONFIG_FAN_PWM_POLARITY, d ? 1 : 0) != 0; }
    inline uint16_t getPidTargetTemp(uint16_t d) { return cfgGetU16(NVS_CONFIG_PID_TARGET_TEMP, d); }
    inline uint16_t getPidP(uint16_t d) { return cfgGetU16(NVS_CONFIG_PID_P, d); }
    inline uint16_t getPidI(uint16_t d) { return cfgGetU16(NVS_CONFIG_PID_I, d); }
    inline uint16_t getPidD(uint16_t d) { return cfgGetU16(NVS_CONFIG_PID_D, d); }
    inline uint32_t getVrFrequency(uint32_t d) { return (uint32_t) cfgGetU64(NVS_CONFIG_VR_FREQUENCY, d); }

    // OTP Replay-Protection state (last_step + 3-bit mask)
    inline void getOTPReplayState(int64_t& base_step, uint8_t& mask) {
        base_step = (int64_t) cfgGetU64(NVS_CONFIG_OTP_LAST_STEP, 0ULL);
        mask      = (uint8_t) cfgGetU16(NVS_CONFIG_OTP_USED_MASK, 0);
    }

    inline void setOTPReplayState(int64_t base_step, uint8_t mask) {
        cfgSetU64(NVS_CONFIG_OTP_LAST_STEP, (uint64_t) base_step);
        cfgSetU16(NVS_CONFIG_OTP_USED_MASK, (uint16_t)(mask & 0x07));
    }

    inline void setOTPSecret(const char* value) { cfgSetStr(NVS_CONFIG_OTP_SECRET, value); }
    inline char* getOTPSecret() { return cfgGetStrAlloc(NVS_CONFIG_OTP_SECRET, ""); }

    inline void setOTTBootId(uint32_t boot_id) { cfgSetU64(NVS_CONFIG_OTP_BOOT_ID, boot_id); }
    inline uint32_t getOTPBootId() { return (uint32_t) cfgGetU64(NVS_CONFIG_OTP_BOOT_ID, 0); }

    inline void setOTPSessionKey(const char* value) { cfgSetStr(NVS_CONFIG_OTP_SESSION_KEY, value); }
    inline char* getOTPSessionKey() { return cfgGetStrAlloc(NVS_CONFIG_OTP_SESSION_KEY, ""); }

    inline void setOTPEnabled(bool value) { cfgSetU16(NVS_CONFIG_OTP_ENABLED, value ? 1 : 0); }
    inline bool isOTPEnabled() { return cfgGetU16(NVS_CONFIG_OTP_ENABLED, 0) != 0; }

    // 0 = disabled, 1 = Basic (address present), 2 = Extended (address present + fee <= maxFee)
    inline void setCoinbaseVerifyMode(int pool, uint16_t v) {
        pool == 0 ? cfgSetU16(NVS_CONFIG_COINBASE_VERIFY_MODE, v)
                  : cfgSetU16(NVS_CONFIG_FB_COINBASE_VERIFY_MODE, v);
    }
    inline uint16_t getCoinbaseVerifyMode(int pool = 0) {
        return pool == 0 ? cfgGetU16(NVS_CONFIG_COINBASE_VERIFY_MODE, 0)
                         : cfgGetU16(NVS_CONFIG_FB_COINBASE_VERIFY_MODE, 0);
    }
    inline void setCoinbaseMaxFee(int pool, uint16_t v) {
        pool == 0 ? cfgSetU16(NVS_CONFIG_COINBASE_MAX_FEE, v)
                  : cfgSetU16(NVS_CONFIG_FB_COINBASE_MAX_FEE, v);
    }
    // stored as tenths of percent: 5 = 0.5%, 30 = 3.0%
    inline uint16_t getCoinbaseMaxFee(int pool = 0) {
        return pool == 0 ? cfgGetU16(NVS_CONFIG_COINBASE_MAX_FEE, 30)
                         : cfgGetU16(NVS_CONFIG_FB_COINBASE_MAX_FEE, 30);
    }
    inline void setCoinbaseVerifyForce(int pool, bool v) {
        pool == 0 ? cfgSetU16(NVS_CONFIG_COINBASE_VERIFY_FORCE, v ? 1 : 0)
                  : cfgSetU16(NVS_CONFIG_FB_COINBASE_VERIFY_FORCE, v ? 1 : 0);
    }
    inline bool getCoinbaseVerifyForce(int pool = 0) {
        return pool == 0 ? cfgGetU16(NVS_CONFIG_COINBASE_VERIFY_FORCE, 0) != 0
                         : cfgGetU16(NVS_CONFIG_FB_COINBASE_VERIFY_FORCE, 0) != 0;
    }

    void migrate_config();
}
