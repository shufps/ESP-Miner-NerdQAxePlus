#include <pthread.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "global_state.h"
#include "nvs_config.h"
#include "influx_task.h"

static const char *TAG = "influx_task";

static Influx *influxdb = 0;

bool last_block_found = false;

// Timer callback function to increment uptime counters
void uptime_timer_callback(TimerHandle_t xTimer)
{
    // Increment uptime counters
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.total_uptime += 1;
    influxdb->stats.uptime += 1;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_temperature(float temp, float temp2)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.temp = temp;
    influxdb->stats.temp2 = temp2;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.pwr_vin = vin;
    influxdb->stats.pwr_iin = iin;
    influxdb->stats.pwr_pin = pin;
    influxdb->stats.pwr_vout = vout;
    influxdb->stats.pwr_iout = iout;
    influxdb->stats.pwr_pout = pout;
    pthread_mutex_unlock(&influxdb->lock);
}

static void influx_task_fetch_from_system_module(System *module)
{
    // fetch best difficulty
    float best_diff = module->getBestSessionNonceDiff();

    influxdb->stats.best_difficulty = best_diff;

    if (best_diff > influxdb->stats.total_best_difficulty) {
        influxdb->stats.total_best_difficulty = best_diff;
    }

    // fetch hashrate
    influxdb->stats.hashing_speed = module->getCurrentHashrate10m();

    // accepted
    influxdb->stats.accepted = module->getSharesAccepted();

    // rejected
    influxdb->stats.not_accepted = module->getSharesRejected();

    // pool errors
    influxdb->stats.pool_errors = module->getPoolErrors();

    // pool difficulty
    influxdb->stats.difficulty = module->getPoolDifficulty();

    // found block
    // firmware sets the flag but never removes it
    // so detect the "edge"
    bool found = module->isFoundBlock();
    if (found && !last_block_found) {
        influxdb->stats.blocks_found++;
        influxdb->stats.total_blocks_found++;
    }
    last_block_found = found;
}

static void forever()
{
    ESP_LOGI(TAG, "halting influx_task");
    while (1) {
        vTaskDelay(15000 / portTICK_PERIOD_MS);
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

    influxdb = influx_init(influxURL, influxPort, influxToken, influxBucket, influxOrg, influxPrefix);

    bool ping_ok = false;
    bool bucket_ok = false;
    bool loaded_values_ok = false;
    // c can be weird at times :weird-smiley-guy:
    while (1) {
        do {
            ping_ok = ping_ok || influx_ping(influxdb);
            if (!ping_ok) {
                ESP_LOGE(TAG, "InfluxDB not reachable!");
                break;
            }

            bucket_ok = bucket_ok || bucket_exists(influxdb);
            if (!bucket_ok) {
                ESP_LOGE(TAG, "Bucket not found!");
                break;
            }

            loaded_values_ok = loaded_values_ok || load_last_values(influxdb);
            if (!loaded_values_ok) {
                ESP_LOGE(TAG, "loading last values failed");
                break;
            }
        } while (0);
        if (loaded_values_ok) {
            break;
        }
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "last values: total_uptime: %d, total_best_difficulty: %.3f, total_blocks_found: %d",
             influxdb->stats.total_uptime, influxdb->stats.total_best_difficulty, influxdb->stats.total_blocks_found);

    // Create and start the uptime timer with a 1-second period
    TimerHandle_t uptime_timer = xTimerCreate("UptimeTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *) 0, uptime_timer_callback);
    if (uptime_timer != NULL) {
        xTimerStart(uptime_timer, 0);
    } else {
        ESP_LOGE(TAG, "Failed to create uptime timer");
        forever();
    }

    while (1) {
        pthread_mutex_lock(&influxdb->lock);
        influx_task_fetch_from_system_module(module);
        influx_write(influxdb);
        pthread_mutex_unlock(&influxdb->lock);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}
