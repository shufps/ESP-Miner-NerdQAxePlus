#include <pthread.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "global_state.h"
#include "nvs_config.h"
#include "ping_task.h"
#include "influx_task.h"

static const char *TAG = "influx_task";

static Influx *influxdb = 0;

int last_block_found = 0;

// Timer callback function to increment uptime counters
void uptime_timer_callback(TimerHandle_t xTimer)
{
    // Increment uptime counters
    pthread_mutex_lock(&influxdb->m_lock);
    influxdb->m_stats.total_uptime += 1;
    influxdb->m_stats.uptime += 1;
    pthread_mutex_unlock(&influxdb->m_lock);
}

void influx_task_set_temperature(float temp, float temp2)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->m_lock);
    influxdb->m_stats.temp = temp;
    influxdb->m_stats.temp2 = temp2;
    pthread_mutex_unlock(&influxdb->m_lock);
}

void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->m_lock);
    influxdb->m_stats.pwr_vin = vin;
    influxdb->m_stats.pwr_iin = iin;
    influxdb->m_stats.pwr_pin = pin;
    influxdb->m_stats.pwr_vout = vout;
    influxdb->m_stats.pwr_iout = iout;
    influxdb->m_stats.pwr_pout = pout;
    pthread_mutex_unlock(&influxdb->m_lock);
}

static void influx_task_fetch_from_system_module(System *module)
{
    // fetch best difficulty
    float best_diff = module->getBestSessionNonceDiff();

    influxdb->m_stats.best_difficulty = best_diff;

    if (best_diff > influxdb->m_stats.total_best_difficulty) {
        influxdb->m_stats.total_best_difficulty = best_diff;
    }

    // fetch hashrate
    influxdb->m_stats.hashing_speed = module->getCurrentHashrate10m();

    // accepted
    influxdb->m_stats.accepted = module->getSharesAccepted();

    // rejected
    influxdb->m_stats.not_accepted = module->getSharesRejected();

    // pool errors
    influxdb->m_stats.pool_errors = module->getPoolErrors();

    // pool difficulty
    influxdb->m_stats.difficulty = module->getPoolDifficulty();

    // Ping RTT
    influxdb->m_stats.last_ping_rtt = get_last_ping_rtt();

    // found blocks
    int found = module->getFoundBlocks();
    if (found && !last_block_found) {
        influxdb->m_stats.blocks_found++;
        influxdb->m_stats.total_blocks_found++;
    }
    last_block_found = found;
}

static void forever()
{
    ESP_LOGI(TAG, "halting influx_task");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

void influx_task(void *pvParameters)
{
    System *module = &SYSTEM_MODULE;

    bool influxEnable = Config::isInfluxEnabled();

    if (!influxEnable) {
        ESP_LOGI(TAG, "InfluxDB is not enabled.");
        forever();
    }

    char *influxURL = Config::getInfluxURL();
    int influxPort = Config::getInfluxPort();
    char *influxToken = Config::getInfluxToken();
    char *influxBucket = Config::getInfluxBucket();
    char *influxOrg = Config::getInfluxOrg();
    char *influxPrefix = Config::getInfluxPrefix();

    ESP_LOGI(TAG, "URL: %s, port: %d, bucket: %s, org: %s, prefix: %s", influxURL, influxPort, influxBucket, influxOrg,
             influxPrefix);

    influxdb = new Influx();
    influxdb->init(influxURL, influxPort, influxToken, influxBucket, influxOrg, influxPrefix);

    bool ping_ok = false;
    bool bucket_ok = false;
    bool loaded_values_ok = false;
    // c can be weird at times :weird-smiley-guy:
    while (1) {
        do {
            ping_ok = ping_ok || influxdb->ping();
            if (!ping_ok) {
                ESP_LOGE(TAG, "InfluxDB not reachable!");
                break;
            }

            bucket_ok = bucket_ok || influxdb->bucket_exists();
            if (!bucket_ok) {
                ESP_LOGE(TAG, "Bucket not found!");
                if (!influxdb->create_bucket()) {
                    ESP_LOGE(TAG, "Bucket couldn't be created!");
                    forever();
                }
                break;
            }

            loaded_values_ok = loaded_values_ok || influxdb->load_last_values();
            if (!loaded_values_ok) {
                ESP_LOGE(TAG, "loading last values failed");
                break;
            }
        } while (0);
        if (loaded_values_ok) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }

    ESP_LOGI(TAG, "last values: total_uptime: %d, total_best_difficulty: %.3f, total_blocks_found: %d",
             influxdb->m_stats.total_uptime, influxdb->m_stats.total_best_difficulty, influxdb->m_stats.total_blocks_found);

    // Create and start the uptime timer with a 1-second period
    TimerHandle_t uptime_timer = xTimerCreate("UptimeTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *) 0, uptime_timer_callback);
    if (uptime_timer != NULL) {
        xTimerStart(uptime_timer, 0);
    } else {
        ESP_LOGE(TAG, "Failed to create uptime timer");
        forever();
    }

    while (1) {
        pthread_mutex_lock(&influxdb->m_lock);
        influx_task_fetch_from_system_module(module);
        influxdb->write();
        pthread_mutex_unlock(&influxdb->m_lock);
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
