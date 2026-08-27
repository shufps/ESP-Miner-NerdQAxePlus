#include <pthread.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "global_state.h"
#include "nvs_config.h"
#include "ping_task.h"
#include "influx_task.h"
#include "stratum/stratum_manager.h"

static const char *TAG = "influx_task";

static Influx *influxdb = 0;

int last_block_found = 0;

uint64_t getDuplicateHWNonces();

// Telemetry accumulators.
//
// The power management task samples the buck every POLL_RATE (2s) and the
// influx task publishes every 15s. Previously each setter overwrote the
// previous value, so ~7 of every 8 samples were discarded -- decimation with
// no anti-alias filtering. Accumulate instead and publish the mean.
typedef struct {
    double vin, iin, pin, vout, iout, pout;
    double temp, temp2;
    double fan_pwm_0, fan_rpm_0, fan_pwm_1, fan_rpm_1;
    uint32_t pwr_count;
    uint32_t temp_count;
    uint32_t fan_count;
} TelemetryAccum;

static TelemetryAccum s_accum;

// must be called with influxdb->m_lock held
static void accum_reset(void)
{
    memset(&s_accum, 0, sizeof(s_accum));
}

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
    s_accum.temp += temp;
    s_accum.temp2 += temp2;
    s_accum.temp_count++;
    pthread_mutex_unlock(&influxdb->m_lock);
}

void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->m_lock);
    s_accum.vin += vin;
    s_accum.iin += iin;
    s_accum.pin += pin;
    s_accum.vout += vout;
    s_accum.iout += iout;
    s_accum.pout += pout;
    s_accum.pwr_count++;
    pthread_mutex_unlock(&influxdb->m_lock);
}

void influx_set_fan(float pwm0, float rpm0, float pwm1, float rpm1) {
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->m_lock);
    s_accum.fan_pwm_0 += pwm0;
    s_accum.fan_rpm_0 += rpm0;
    s_accum.fan_pwm_1 += pwm1;
    s_accum.fan_rpm_1 += rpm1;
    s_accum.fan_count++;
    pthread_mutex_unlock(&influxdb->m_lock);
}

static void influx_task_fetch_from_stratum_manager(StratumManager *module) {
    // fetch best difficulty
    float best_diff = module->getBestSessionDiff();

    influxdb->m_stats.best_difficulty = best_diff;

    if (best_diff > influxdb->m_stats.total_best_difficulty) {
        influxdb->m_stats.total_best_difficulty = best_diff;
    }

    // accepted
    influxdb->m_stats.accepted = module->getSharesAccepted();

    // rejected
    influxdb->m_stats.not_accepted = module->getSharesRejected();

    // duplicate
    influxdb->m_stats.duplicate_hashes = getDuplicateHWNonces();

    // pool errors
    influxdb->m_stats.pool_errors = module->getPoolErrors();

    // pool difficulty
    influxdb->m_stats.difficulty = module->getPoolDifficulty();

    // found blocks
    int found = module->getFoundBlocks();
    if (found && !last_block_found) {
        influxdb->m_stats.blocks_found++;
        influxdb->m_stats.total_blocks_found++;
    }
    last_block_found = found;
}

// Averages everything collected since the last write and stores it into
// m_stats. If no samples arrived (buck not initialised, shutdown) the previous
// values are left in place.
//
// Returns false when nothing at all was collected since the last publish. That
// happens on the first pass after boot, before the power management task has run
// once: m_stats still holds its zero-initialised values, so publishing would emit
// an all-zero record. The caller skips the write in that case.
// must be called with influxdb->m_lock held
static bool influx_task_flush_accum(void)
{
    const bool have_samples = s_accum.pwr_count || s_accum.temp_count || s_accum.fan_count;

    if (s_accum.pwr_count) {
        const double n = (double) s_accum.pwr_count;
        influxdb->m_stats.pwr_vin = (float) (s_accum.vin / n);
        influxdb->m_stats.pwr_iin = (float) (s_accum.iin / n);
        influxdb->m_stats.pwr_pin = (float) (s_accum.pin / n);
        influxdb->m_stats.pwr_vout = (float) (s_accum.vout / n);
        influxdb->m_stats.pwr_iout = (float) (s_accum.iout / n);
        influxdb->m_stats.pwr_pout = (float) (s_accum.pout / n);
    }

    if (s_accum.temp_count) {
        const double n = (double) s_accum.temp_count;
        influxdb->m_stats.temp = (float) (s_accum.temp / n);
        influxdb->m_stats.temp2 = (float) (s_accum.temp2 / n);
    }

    if (s_accum.fan_count) {
        const double n = (double) s_accum.fan_count;
        influxdb->m_stats.fan_pwm_0 = (float) (s_accum.fan_pwm_0 / n);
        influxdb->m_stats.fan_rpm_0 = (float) (s_accum.fan_rpm_0 / n);
        influxdb->m_stats.fan_pwm_1 = (float) (s_accum.fan_pwm_1 / n);
        influxdb->m_stats.fan_rpm_1 = (float) (s_accum.fan_rpm_1 / n);
    }

    ESP_LOGD(TAG, "flushed telemetry: %lu pwr, %lu temp, %lu fan samples", s_accum.pwr_count, s_accum.temp_count,
             s_accum.fan_count);

    accum_reset();
    return have_samples;
}

static void influx_task_fetch_from_system_module(System *module)
{
    // fetch hashrate
    influxdb->m_stats.hashing_speed = module->getCurrentHashrate();
    influxdb->m_stats.hashing_speed_1m = module->getCurrentHashrate1m();

    // Ping RTT
    influxdb->m_stats.last_ping_rtt = get_last_ping_rtt();

    // Recent ping packet loss ratio
    influxdb->m_stats.recent_ping_loss = get_recent_ping_loss();
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

    accum_reset();

    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(TAG, "suspended");
            vTaskSuspend(NULL);
        }
        // Averaging + snapshot happen under the lock (fast, in-memory); the
        // blocking HTTP write() is done AFTER unlocking, on a private copy, so a
        // slow/unreachable InfluxDB can never stall the power-management task,
        // which takes the same lock from its telemetry setters.
        Stats snapshot;
        pthread_mutex_lock(&influxdb->m_lock);
        influx_task_fetch_from_system_module(module);
        influx_task_fetch_from_stratum_manager(STRATUM_MANAGER);
        bool have_samples = influx_task_flush_accum();
        snapshot = influxdb->m_stats;
        pthread_mutex_unlock(&influxdb->m_lock);

        if (have_samples) {
            influxdb->write(snapshot);
        } else {
            ESP_LOGI(TAG, "no telemetry collected yet, skipping write");
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
