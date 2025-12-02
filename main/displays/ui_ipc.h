// ui_ipc.h
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
    UI_CMD_SHOW_QR,
    UI_CMD_HIDE_QR
} ui_cmd_t;

typedef struct
{
    ui_cmd_t type;
    void *payload;  // e.g., char* uri (heap)
    uint32_t param; // e.g., max_px or timeout
} ui_msg_t;

extern QueueHandle_t g_ui_queue;

void ui_ipc_init(void);
bool ui_send_show_qr();
bool ui_send_hide_qr();