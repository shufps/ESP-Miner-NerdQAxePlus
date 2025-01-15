#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h" // For sample Wi‑Fi connectivity checking
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "ntp_time.h"
#include "nvs_config.h"

static const char *TAG = "ntp_time";

NTPTime::NTPTime()
{
    m_valid = false;
}

/*
 * Initialize SNTP by setting its operating mode and specifying the NTP server.
 * Once initialized, the SNTP client will automatically update the system clock.
 */
void NTPTime::initialize_sntp(const char *host)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, host);
    sntp_init();
}

void NTPTime::taskWrapper(void *pvParameters)
{
    NTPTime *timeInstance = static_cast<NTPTime *>(pvParameters);
    timeInstance->task();
}

void NTPTime::task()
{
    char *ntp = nvs_config_get_string(NVS_CONFIG_NTP, CONFIG_ESP_NTP_SERVER);

    ESP_LOGI(TAG, "NTP server: %s", ntp);

    initialize_sntp(ntp);

    time_t epoch = 0;
    while (1) {
        // get system time
        time(&epoch);

        // epoch > 1.1.2025 0:00:00?
        if ((uint32_t) epoch > 1735689600) {
            m_valid = true;
            break;
        }
        ESP_LOGI(TAG, "waiting for time ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "set epoch to %lu", (uint32_t) epoch);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
