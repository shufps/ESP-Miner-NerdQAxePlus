#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

#include "asic_task.h"
#include "bm1368.h"
#include "common.h"
#include "power_management_task.h"
#include "serial.h"
#include "stratum_api.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define STRATUM_USER CONFIG_STRATUM_USER

#define DIFF_STRING_SIZE 12

#define MAX_ASIC_JOBS 128

typedef enum
{
    DEVICE_UNKNOWN = -1,
    DEVICE_NERDQAXE_PLUS,
} DeviceModel;

typedef enum
{
    ASIC_UNKNOWN = -1,
    ASIC_BM1368,
} AsicModel;

typedef struct
{
    uint8_t (*init_fn)(uint64_t, uint16_t);
    void (*receive_result_fn)(task_result *result);
    int (*set_max_baud_fn)(void);
    void (*set_difficulty_mask_fn)(int);
    uint8_t (*send_work_fn)(uint32_t jobid, bm_job *next_bm_job);
    bool (*send_hash_frequency_fn)(float frequency);
} AsicFunctions;

typedef struct
{
    double current_hashrate_10m;
    int64_t start_time;
    uint64_t shares_accepted;
    uint64_t shares_rejected;
    int screen_page;
    char oled_buf[20];
    uint64_t best_nonce_diff;
    char best_diff_string[DIFF_STRING_SIZE];
    uint64_t best_session_nonce_diff;
    char best_session_diff_string[DIFF_STRING_SIZE];
    bool FOUND_BLOCK;
    bool startup_done;
    char ssid[32];
    char wifi_status[20];
    char *pool_url;
    uint16_t pool_port;
    uint32_t pool_difficulty;

    int pool_errors;

    uint32_t lastClockSync;
} SystemModule;

typedef struct
{
    DeviceModel device_model;
    char *device_model_str;
    int board_version;
    AsicModel asic_model;
    char *asic_model_str;
    uint16_t asic_count;
    uint16_t voltage_domain;
    AsicFunctions ASIC_functions;
    double asic_job_frequency_ms;
    uint32_t initial_ASIC_difficulty;

    SystemModule SYSTEM_MODULE;
    AsicTaskModule ASIC_TASK_MODULE;
    PowerManagementModule POWER_MANAGEMENT_MODULE;

    uint8_t valid_jobs[MAX_ASIC_JOBS];
    pthread_mutex_t valid_jobs_lock;

    int sock;

} GlobalState;

#endif /* GLOBAL_STATE_H_ */
