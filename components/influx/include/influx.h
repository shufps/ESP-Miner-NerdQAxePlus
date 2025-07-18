#pragma once

#include <pthread.h>

typedef struct
{
    float temp;
    float temp2;
    float hashing_speed;
    int invalid_shares; // not implemented
    int valid_shares;   // not implemented
    int difficulty;
    float best_difficulty;
    int pool_errors;
    int accepted;
    int not_accepted;
    int total_uptime;
    float total_best_difficulty;
    int uptime;
    int blocks_found;
    int total_blocks_found;
    int duplicate_hashes; // not implemented
    float pwr_vin;
    float pwr_iin;
    float pwr_pin;
    float pwr_vout;
    float pwr_iout;
    float pwr_pout;
    float last_ping_rtt; 
} Stats;

class Influx {
  protected:
    char *m_host;
    int m_port;
    char *m_token;
    char *m_org;
    char *m_bucket;
    char *m_prefix;

    char m_auth_header[128];
    char *m_big_buffer;

    bool get_org_id(char *out_org_id, size_t max_len);

  public:
    // make this beautiful later
    Stats m_stats;
    pthread_mutex_t m_lock;

    Influx();

    bool init(const char *host, int port, const char *token, const char *bucket, const char *org, const char *prefix);
    void write();
    bool load_last_values();
    bool bucket_exists();
    bool create_bucket();
    bool ping();
};
