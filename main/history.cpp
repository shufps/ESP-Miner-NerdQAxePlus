#include <math.h>
#include <pthread.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h" // Include esp_timer for esp_timer_get_time
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "global_state.h"
#include "history.h"
#include "macros.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wmissing-prototypes"

static const char *TAG = "history";

// define for wrapped access of psram
#define WRAP(a) ((a) & (HISTORY_MAX_SAMPLES - 1))

NonceDistribution::NonceDistribution()
{
    // NOP
}

void NonceDistribution::init(int numAsics)
{
    m_numAsics = numAsics;
    if (m_numAsics) {
        m_distribution = (uint32_t *) calloc(m_numAsics, sizeof(uint32_t));
    }
}

void NonceDistribution::addShare(int asicNr)
{
    if (m_distribution && asicNr < m_numAsics) {
        m_distribution[asicNr]++;
    }
}

void NonceDistribution::toLog()
{
    // this can happen if we don't have asics
    if (!m_distribution) {
        return;
    }

    char buffer[256];
    size_t offset = 0;

    // Iterate through each ASIC and append its count to the log message
    for (int i = 0; i < m_numAsics; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%lu/", m_distribution[i]);
    }
    if (offset > 0) {
        buffer[offset - 1] = 0; // remove trailing slash
    }

    ESP_LOGI(TAG, "nonce distribution: %s", buffer);
}

uint64_t History::getTimestampSample(int index)
{
    return m_timestamps[WRAP(index)];
}

float History::getHashrate1mSample(int index)
{
    return m_hashrate1m[WRAP(index)];
}

float History::getHashrate10mSample(int index)
{
    return m_hashrate10m[WRAP(index)];
}

float History::getHashrate1hSample(int index)
{
    return m_hashrate1h[WRAP(index)];
}

float History::getHashrate1dSample(int index)
{
    return m_hashrate1d[WRAP(index)];
}

double History::getCurrentHashrate1m()
{
    return m_avg1m.getGhDisplay();
}

double History::getCurrentHashrate10m()
{
    return m_avg10m.getGhDisplay();
}

double History::getCurrentHashrate1h()
{
    return m_avg1h.getGhDisplay();
}

double History::getCurrentHashrate1d()
{
    return m_avg1d.getGhDisplay();
}

uint32_t History::getRateSample(int index)
{
    return m_rates[WRAP(index)];
}

uint64_t History::getCurrentTimestamp()
{
    // all timestamps are equal
    return m_avg10m.getTimestamp();
}

void History::lock()
{
    pthread_mutex_lock(&m_mutex);
}

void History::unlock()
{
    pthread_mutex_unlock(&m_mutex);
}

bool History::isAvailable()
{
    return m_rates && m_timestamps && m_hashrate1m && m_hashrate10m && m_hashrate1h && m_hashrate1d;
}

History::History()
    : m_avg1m(this, 60llu * 1000llu, HR_INTERVAL), m_avg10m(this, 600llu * 1000llu, HR_INTERVAL), m_avg1h(this, 3600llu * 1000llu, HR_INTERVAL),
      m_avg1d(this, 86400llu * 1000llu, HR_INTERVAL)
{
    // NOP
}

bool History::init(int num_asics)
{
    m_rates = (uint32_t *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(uint32_t));
    m_timestamps = (uint64_t *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(uint64_t));
    m_hashrate1m = (float *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(float));
    m_hashrate10m = (float *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(float));
    m_hashrate1h = (float *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(float));
    m_hashrate1d = (float *) CALLOC(HISTORY_MAX_SAMPLES, sizeof(float));

    ESP_LOGI(TAG, "History size %d samples", (int) HISTORY_MAX_SAMPLES);

    m_distribution.init(num_asics);

    return isAvailable();
}

void History::getTimestamps(uint64_t *first, uint64_t *last, int *num_samples)
{
    int lowest_index = (m_numSamples - HISTORY_MAX_SAMPLES < 0) ? 0 : m_numSamples - HISTORY_MAX_SAMPLES;
    int highest_index = m_numSamples - 1;

    int _num_samples = highest_index - lowest_index + 1;

    uint64_t first_timestamp = (_num_samples) ? getTimestampSample(lowest_index) : 0;
    uint64_t last_timestamp = (_num_samples) ? getTimestampSample(highest_index) : 0;

    *first = first_timestamp;
    *last = last_timestamp;
    *num_samples = _num_samples;
}

HistoryAvg::HistoryAvg(History *history, uint64_t timespan, uint32_t samplePeriodMs)
    : m_timespan(timespan), m_samplePeriodMs(samplePeriodMs), m_history(history)
{
    // NOP
}

// Call on each push to include new samples and trim the left edge
void HistoryAvg::update()
{
    // 1) Pull in newly arrived samples on the right edge
    const int histCount = m_history->getNumSamples();
    while (m_lastSample + 1 < histCount) {
        m_lastSample++;
        m_numSamples++;
        m_sumRates += (int64_t) m_history->getRateSample(m_lastSample);
    }

    if (m_lastSample < 0 || m_numSamples <= 0) {
        // No data yet
        m_timestamp = 0;
        m_avgGh = 0.0;
        m_avgGhDisplay = 0.0;
        m_preliminary = true;
        return;
    }

    const uint64_t lastTs = m_history->getTimestampSample(m_lastSample);

    // 2) Trim from the left while the first sample is too old for the time window
    //    Keep the window such that (lastTs - firstTs) < m_timespan whenever possible.
    while (m_firstSample < m_lastSample && lastTs - m_history->getTimestampSample(m_firstSample) >= m_timespan) {
        m_sumRates -= (int64_t) m_history->getRateSample(m_firstSample);
        m_numSamples--;
        m_firstSample++;
    }

    const uint64_t firstTs = m_history->getTimestampSample(m_firstSample);
    const uint64_t covered = (lastTs >= firstTs) ? (lastTs - firstTs) : 0;

    // Consider the window "filled" once we effectively have >= timespan coverage.
    // Add one samplePeriod as tolerance to account for discrete sampling.
    const bool filled = (covered + m_samplePeriodMs) >= m_timespan;

    // preliminary only while the window is not filled yet
    m_preliminary = !filled;

    // Unbiased average over actual samples in the window (Q10 -> GH/s)
    const int denom = (m_numSamples > 0) ? m_numSamples : 1;
    const double avg_q10 = (double) m_sumRates / (double) denom;
    m_avgGh = avg_q10 / 1024.0; // -> GH/s

    // Display ("ramping") average: only damp while preliminary
    if (m_preliminary) {
        // scale by coverage ratio with the same tolerance
        double fill = (double) (covered + m_samplePeriodMs) / (double) m_timespan;
        if (fill > 1.0)
            fill = 1.0;
        m_avgGhDisplay = m_avgGh * fill;
    } else {
        m_avgGhDisplay = m_avgGh;
    }

    m_timestamp = lastTs;
}
// push a measured instantaneous hashrate (GH/s)
void History::pushRate(float rateGh, uint64_t timestamp)
{
    if (!isAvailable()) {
        ESP_LOGW(TAG, "PSRAM not initialized");
        return;
    }

    lock();

    // sanity check
    if (!isfinite(rateGh) || rateGh < 0.0f)
        rateGh = 0.0f;

    // store rate sample and timestamp
    m_rates[WRAP(m_numSamples)] = (uint32_t) (rateGh * 1024.0); // -> Q22.10
    m_timestamps[WRAP(m_numSamples)] = timestamp;
    m_numSamples++;

    // update all windows
    m_avg1m.update();
    m_avg10m.update();
    m_avg1h.update();
    m_avg1d.update();

    // write per-sample windowed averages for export
    m_hashrate1m[WRAP(m_numSamples - 1)] = m_avg1m.getGhDisplay();
    m_hashrate10m[WRAP(m_numSamples - 1)] = m_avg10m.getGhDisplay();
    m_hashrate1h[WRAP(m_numSamples - 1)] = m_avg1h.getGhDisplay();
    m_hashrate1d[WRAP(m_numSamples - 1)] = m_avg1d.getGhDisplay();

    unlock();

    char p1 = (m_avg1m.isPreliminary()) ? '*' : ' ';
    char p2 = (m_avg10m.isPreliminary()) ? '*' : ' ';
    char p3 = (m_avg1h.isPreliminary()) ? '*' : ' ';
    char p4 = (m_avg1d.isPreliminary()) ? '*' : ' ';
    ESP_LOGI(TAG, "hashrate: 1m:%.3fGH%c 10m:%.3fGH%c 1h:%.3fGH%c 1d:%.3fGH%c", m_avg1m.getGh(), p1, m_avg10m.getGh(), p2,
             m_avg1h.getGh(), p3, m_avg1d.getGh(), p4);
}

void History::pushShare(int asic_nr)
{
    lock();

    m_distribution.addShare(asic_nr);

    unlock();
}

// successive approximation in a wrapped ring buffer with
// monotonic/unwrapped write pointer :woozy:
int History::searchNearestTimestamp(int64_t timestamp)
{
    // get index of the first sample, clamp to min 0
    int lowest_index = (m_numSamples - HISTORY_MAX_SAMPLES < 0) ? 0 : m_numSamples - HISTORY_MAX_SAMPLES;

    // last sample
    int highest_index = m_numSamples - 1;

    ESP_LOGD(TAG, "lowest_index: %d highest_index: %d", lowest_index, highest_index);

    int current = 0;
    int num_elements = 0;

    while (current = (highest_index + lowest_index) / 2, num_elements = highest_index - lowest_index + 1, num_elements > 1) {
        // Get timestamp at the current index, wrapping as necessary
        uint64_t stored_timestamp = getTimestampSample(current);
        ESP_LOGD(TAG, "current %d num_elements %d stored_timestamp %llu wrapped-current %d", current, num_elements,
                 stored_timestamp, WRAP(current));

        if ((int64_t) stored_timestamp > timestamp) {
            // If timestamp is too large, search lower
            highest_index = current - 1; // Narrow the search to the lower half
        } else if ((int64_t) stored_timestamp < timestamp) {
            // If timestamp is too small, search higher
            lowest_index = current + 1; // Narrow the search to the upper half
        } else {
            // Exact match found
            return current;
        }
    }

    ESP_LOGD(TAG, "current return %d", current);

    if (current < 0 || current >= m_numSamples) {
        return -1;
    }

    return current;
}


// Helper: fills a JsonObject with history data using ArduinoJson
void History::exportHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp,
                                uint64_t current_timestamp)
{
    // Ensure consistency
    lock();

    int64_t rel_start = (int64_t) start_timestamp - (int64_t) current_timestamp;
    int64_t rel_end = (int64_t) end_timestamp - (int64_t) current_timestamp;

    // Get current system timestamp (in ms)
    uint64_t sys_timestamp = esp_timer_get_time() / 1000ULL;
    int64_t sys_start = (int64_t) sys_timestamp + rel_start;
    int64_t sys_end = (int64_t) sys_timestamp + rel_end;

    int start_index = searchNearestTimestamp(sys_start);
    int end_index = searchNearestTimestamp(sys_end);
    int num_samples = end_index - start_index + 1;

    if (!isAvailable() || start_index == -1 || end_index == -1 || num_samples <= 0 || (end_index < start_index)) {
        ESP_LOGW(TAG, "Invalid history indices or history not (yet) available");
        // If the data is invalid, return an empty object
        num_samples = 0;
    }

    // Create arrays for history samples using the new method
    JsonArray hashrate_1m = json_history["hashrate_1m"].to<JsonArray>();
    JsonArray hashrate_10m = json_history["hashrate_10m"].to<JsonArray>();
    JsonArray hashrate_1h = json_history["hashrate_1h"].to<JsonArray>();
    JsonArray hashrate_1d = json_history["hashrate_1d"].to<JsonArray>();
    JsonArray timestamps = json_history["timestamps"].to<JsonArray>();

    for (int i = start_index; i < start_index + num_samples; i++) {
        uint64_t sample_timestamp = getTimestampSample(i);
        if ((int64_t) sample_timestamp < sys_start) {
            continue;
        }
        // Multiply by 100.0 and cast to int as in the original code
        hashrate_1m.add((int) (getHashrate1mSample(i) * 100.0));
        hashrate_10m.add((int) (getHashrate10mSample(i) * 100.0));
        hashrate_1h.add((int) (getHashrate1hSample(i) * 100.0));
        hashrate_1d.add((int) (getHashrate1dSample(i) * 100.0));
        timestamps.add((int64_t) sample_timestamp - sys_start);
    }

    // Add base timestamp for reference
    json_history["timestampBase"] = start_timestamp;

    unlock();
}
