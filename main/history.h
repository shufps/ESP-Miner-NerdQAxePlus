#pragma once

#include "ArduinoJson.h"

#include "esp_psram.h"

// must be power of two
// is enough for 1d
#define HISTORY_MAX_SAMPLES 131072

class History;

class NonceDistribution {
  protected:
    int m_numAsics;
    uint32_t *m_distribution = nullptr;

  public:
    NonceDistribution();
    void init(int numAsics);
    void addShare(int asicNr);
    void toLog();
};

// --- HistoryAvg: robust, time-windowed, with "ramping" display average ---
class HistoryAvg {
  protected:
    int m_firstSample = 0;
    int m_lastSample  = -1;           // start "empty"
    uint64_t m_timespan = 0;          // desired window length in ms
    uint32_t m_samplePeriodMs = 5000; // your push period (5s)
    int m_numSamples = 0;             // actual samples inside the window

    int64_t m_sumRates = 0;           // sum of rate samples in Q10 (uQ22.10 summed into sQ54.10)
    double m_avgGh = 0.0;             // unbiased average (GH/s)
    double m_avgGhDisplay = 0.0;      // "ramping" average for UI (GH/s)
    uint64_t m_timestamp = 0;         // timestamp of the newest sample in the window
    bool m_preliminary = true;        // duration < timespan

    History *m_history = nullptr;

  public:
    HistoryAvg(History *history, uint64_t timespan, uint32_t samplePeriodMs = 5000);

    // Call on each push to include new samples and trim the left edge
    void update();

    // True, physically correct average GH/s
    float getGh() const { return (float)m_avgGh; }

    // Smoothed, "ramping" average GH/s for plotting or UI
    float getGhDisplay() const { return (float)m_avgGhDisplay; }

    uint64_t getTimestamp() const { return m_timestamp; }
    bool isPreliminary() const { return m_preliminary; }
};



class History {
  protected:
    int m_numSamples = 0;
    uint32_t *m_rates = nullptr; // (uQ22.10)
    uint64_t *m_timestamps = nullptr;
    float *m_hashrate1m = nullptr;
    float *m_hashrate10m = nullptr;
    float *m_hashrate1h = nullptr;
    float *m_hashrate1d = nullptr;

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

    HistoryAvg m_avg1m;   // 1-minute average for real-time monitoring
    HistoryAvg m_avg10m;
    HistoryAvg m_avg1h;
    HistoryAvg m_avg1d;
    NonceDistribution m_distribution;

  public:
    History();
    bool init(int numAsics);
    bool isAvailable();
    void getTimestamps(uint64_t *first, uint64_t *last, int *num_samples);
    void pushShare(int asic_nr);
    void pushRate(float rateGh, uint64_t timestamp);

    void lock();
    void unlock();

    uint64_t getTimestampSample(int index);
    float getHashrate1mSample(int index);
    float getHashrate10mSample(int index);
    float getHashrate1hSample(int index);
    float getHashrate1dSample(int index);
    uint64_t getCurrentTimestamp(void);
    double getCurrentHashrate1m();   // 1-minute average for real-time monitoring
    double getCurrentHashrate10m();
    double getCurrentHashrate1h();
    double getCurrentHashrate1d();
    uint32_t getRateSample(int index);
    int searchNearestTimestamp(int64_t timestamp);

    void exportHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp);

    int getNumSamples()
    {
        return m_numSamples;
    };
};
