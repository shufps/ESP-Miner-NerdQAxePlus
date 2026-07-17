#pragma once
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

class StratumManager;

struct PingHistory {
    uint16_t sent;
    uint16_t received;
};

// Result structure used by ping logic
struct PingResult {
    bool success;
    uint16_t replies;
    double avg_rtt_ms;
    double min_rtt_ms;
    double max_rtt_ms;
};

struct PingStats
{
    uint16_t replies;
    uint32_t total_time_ms;
    double min_rtt;
    double max_rtt;
    char hostname[64];
    bool header_shown;
    const char* tag;
};

class PingTask {
  protected:
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
    int m_pool = 0;
    PingHistory *m_ping_history = nullptr;
    double m_last_ping_rtt_ms = 0.0;
    int m_history_index = 0;
    int m_history_count = 0;
    const char* m_tag = nullptr;
    StratumManager *m_manager = nullptr;

    // callback context for esp_ping; must not live on perform_ping's stack
    // because the ping thread can still fire callbacks after
    // esp_ping_stop()/esp_ping_delete_session() returned
    PingStats m_stats = {};

    void record_ping_result(uint16_t sent, uint16_t received);
    int init_ping_history();
    double get_recent_packet_loss();
    PingResult perform_ping(const char* ip_str, const char* hostname_str);

  public:
    PingTask(StratumManager *manager, int pool) : m_pool(pool), m_manager(manager) {
        if (!pool) {
            m_tag = "ping task (pri)";
        } else {
            m_tag = "ping task (sec)";
        }
    }

    void reset();
    void ping_task();
    static void ping_task_wrapper(void *pvParameters);

    double get_last_ping_rtt();
    double get_recent_ping_loss();
};