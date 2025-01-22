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

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wmissing-prototypes"

static const char *TAG = "history";

// define for wrapped access of psram
#define WRAP(a) ((a) & (HISTORY_MAX_SAMPLES - 1))


NonceDistribution::NonceDistribution() {
    // NOP
}

void NonceDistribution::init(int numAsics)
{
    m_numAsics = numAsics;
    if (m_numAsics) {
        m_distribution = (uint32_t *) calloc(m_numAsics, sizeof(uint32_t));
    }
}

void NonceDistribution::addShare(int asicNr) {
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

double History::getCurrentHashrate10m()
{
    return m_avg10m.getGh();
}

double History::getCurrentHashrate1h()
{
    return m_avg10m.getGh();
}

double History::getCurrentHashrate1d()
{
    return m_avg10m.getGh();
}

uint32_t History::getShareSample(int index)
{
    return m_shares[WRAP(index)];
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
    return m_shares && m_timestamps && m_hashrate10m && m_hashrate1h && m_hashrate1d;
}

History::History() : m_avg10m(this, 600llu * 1000llu), m_avg1h(this, 3600llu * 1000llu), m_avg1d(this, 86400llu * 1000llu)
{
    // NOP
}

bool History::init(int num_asics)
{
    m_shares = (uint32_t *) heap_caps_malloc(HISTORY_MAX_SAMPLES * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    m_timestamps = (uint64_t *) heap_caps_malloc(HISTORY_MAX_SAMPLES * sizeof(uint64_t), MALLOC_CAP_SPIRAM);
    m_hashrate10m = (float *) heap_caps_malloc(HISTORY_MAX_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);
    m_hashrate1h = (float *) heap_caps_malloc(HISTORY_MAX_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);
    m_hashrate1d = (float *) heap_caps_malloc(HISTORY_MAX_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);

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

HistoryAvg::HistoryAvg(History *history, uint64_t timespan)
{
    m_history = history;
    m_timespan = timespan;
}

// move avg window and track and adjust the total sum of all shares in the
// desired time window. Calculates GH.
// calculates incrementally without "scanning" the entire time span
void HistoryAvg::update()
{
    // Catch up with the latest sample and update diffsum
    uint64_t lastTimestamp = 0;
    while (lastTimestamp = m_history->getTimestampSample(m_lastSample), m_lastSample + 1 < m_history->getNumSamples()) {
        m_lastSample++;
        m_diffSum += (uint64_t) m_history->getShareSample(m_lastSample);
    }

    // adjust the window on the older side
    // we move the lower window bound until the next sample would be out of
    // the desired timespan.
    while (m_firstSample + 1 < m_lastSample && lastTimestamp - m_history->getTimestampSample(m_firstSample + 1) >= m_timespan) {
        m_diffSum -= (uint64_t) m_history->getShareSample(m_firstSample);
        m_firstSample++;
    }

    uint64_t firstTimestamp = m_history->getTimestampSample(m_firstSample);

    // Check for overflow in diffsum
    if (m_diffSum >> 63ull) {
        ESP_LOGE(TAG, "Error in hashrate calculation: diffsum overflowed");
        return;
    }

    // Prevent division by zero
    if (lastTimestamp == firstTimestamp) {
        ESP_LOGW(TAG, "Timestamps are equal; cannot compute average.");
        return;
    }

    // Calculate the average hash rate
    uint64_t duration = (lastTimestamp - firstTimestamp);

    // preliminary means that it's not the real hashrate because
    // it's ramping up slowly
    m_preliminary = duration < m_timespan;

    // clamp duration to a minimum value of avg->timespan
    duration = (m_timespan > duration) ? m_timespan : duration;

    m_avg = (double) (m_diffSum << 32llu) / ((double) duration / 1.0e3);
    m_avgGh = m_avg / 1.0e9;
    m_timestamp = lastTimestamp;
}


void History::pushShare(uint32_t diff, uint64_t timestamp, int asic_nr)
{
    if (!isAvailable()) {
        ESP_LOGW(TAG, "PSRAM not initialized");
        return;
    }

    lock();
    m_shares[WRAP(m_numSamples)] = diff;
    m_timestamps[WRAP(m_numSamples)] = timestamp;
    m_numSamples++;

    m_avg10m.update();
    m_avg1h.update();
    m_avg1d.update();

    m_hashrate10m[WRAP(m_numSamples - 1)] = m_avg10m.getGh();
    m_hashrate1h[WRAP(m_numSamples - 1)] = m_avg1h.getGh();
    m_hashrate1d[WRAP(m_numSamples - 1)] = m_avg1d.getGh();

    m_distribution.addShare(asic_nr);

    unlock();

    char preliminary_10m = (m_avg10m.isPreliminary()) ? '*' : ' ';
    char preliminary_1h = (m_avg1h.isPreliminary()) ? '*' : ' ';
    char preliminary_1d = (m_avg1d.isPreliminary()) ? '*' : ' ';

    ESP_LOGI(TAG, "hashrate: 10m:%.3fGH%c 1h:%.3fGH%c 1d:%.3fGH%c", m_avg10m.getGh(), preliminary_10m, m_avg1h.getGh(),
             preliminary_1h, m_avg1d.getGh(), preliminary_1d);

    m_distribution.toLog();
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
