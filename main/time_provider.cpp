#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h" // For sample Wi‑Fi connectivity checking
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "time_provider.h"
#include "nvs_config.h"

static const char *TAG = "ntp_time";

TimeProvider::TimeProvider()
{
    m_synced = false;
    m_lastNTimeClockSync = 0;

    m_NTPHost = nvs_config_get_string(NVS_CONFIG_NTP, CONFIG_ESP_NTP_SERVER);

    // if we have an empty host we fall-back to ntime syncing
    if (!strlen(m_NTPHost)) {
        m_timeProvider = eTimeProvider::NTIME;
        ESP_LOGI(TAG, "NTIME Time Provider selected");
    } else {
        m_timeProvider = eTimeProvider::NTP;
        ESP_LOGI(TAG, "NTP Time Provider selected");
    }
}

void TimeProvider::setNTime(uint32_t ntime)
{
    // is ntime provider selected?
    if (m_timeProvider != eTimeProvider::NTIME) {
        return;
    }

    // only update epoch by ntime once per hour
    if (ntime < m_lastNTimeClockSync + (60 * 60)) {
        return;
    }

    // get current time as epoch
    time_t epoch = 0;
    time(&epoch);

    // don't adjust the time backwards
    if (ntime < (uint32_t) epoch) {
        return;
    }

    // set the new epoch
    ESP_LOGI(TAG, "Syncing clock from ntime");
    m_lastNTimeClockSync = ntime;
    struct timeval tv;
    tv.tv_sec = ntime;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

void TimeProvider::taskWrapper(void *pvParameters)
{
    TimeProvider *timeInstance = static_cast<TimeProvider *>(pvParameters);
    timeInstance->task();
}

void TimeProvider::task()
{
    if (m_timeProvider == eTimeProvider::NTP) {
        ESP_LOGI(TAG, "Initializing SNTP");
        ESP_LOGI(TAG, "NTP server: %s", m_NTPHost);
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, m_NTPHost);
        sntp_init();
    }

    time_t epoch = 0;
    while (1) {
        // get system time
        time(&epoch);

        // epoch > 1.1.2025 0:00:00?
        if ((uint32_t) epoch > 1735689600) {
            m_synced = true;
            break;
        }
        ESP_LOGI(TAG, "waiting for time ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "epoch: %lu", (uint32_t) epoch);

    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
