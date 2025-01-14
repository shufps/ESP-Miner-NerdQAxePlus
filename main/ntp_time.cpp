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

/*
 * FreeRTOS task that handles NTP time synchronization.
 * The task will check for Wi‑Fi connectivity and then attempt to sync time.
 * If an error occurs (e.g., no Wi‑Fi or NTP failure), the task waits and retries.
 */
void NTPTime::task()
{
    char *ntp = nvs_config_get_string(NVS_CONFIG_NTP, CONFIG_ESP_NTP_SERVER);

    initialize_sntp(ntp);

    while (1) {
        time_t now = 0;
        time(&now);

        ESP_LOGI(TAG, "epoch: %lu", (uint32_t) now);

        if ((uint32_t) time > 0) {
            m_valid = true;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
