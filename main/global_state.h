#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

#include "bm1368.h"
#include "common.h"
#include "tasks/asic_task.h"
#include "tasks/power_management_task.h"
//#include "serial.h"
#include "stratum_api.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define STRATUM_USER CONFIG_STRATUM_USER

#define DIFF_STRING_SIZE 12

#define MAX_ASIC_JOBS 128

#define OVERHEAT_DEFAULT 70

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
    char ssid[33]; // +1 zero terminator
    char wifi_status[20];
    char *pool_url;
    uint16_t pool_port;
    uint32_t pool_difficulty;

    int pool_errors;
    bool overheated;

    uint32_t lastClockSync;
} SystemModule;

extern volatile SystemModule SYSTEM_MODULE;
extern volatile AsicTaskModule ASIC_TASK_MODULE;
extern volatile PowerManagementModule POWER_MANAGEMENT_MODULE;

#endif /* GLOBAL_STATE_H_ */
