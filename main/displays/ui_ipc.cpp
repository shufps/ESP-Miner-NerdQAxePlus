#include <string.h>
#include <stdlib.h>

#include "ui_ipc.h"

QueueHandle_t g_ui_queue = NULL;

void ui_ipc_init(void) {
    g_ui_queue = xQueueCreate(/*len*/ 8, sizeof(ui_msg_t));
}

bool ui_send_show_qr() {
    ui_msg_t m = { .type = UI_CMD_SHOW_QR, .payload = nullptr, .param = 0 };
    if (xQueueSend(g_ui_queue, &m, 0) != pdTRUE) {
        return false;
    }
    return true;
}

bool ui_send_hide_qr() {
    ui_msg_t m = { .type = UI_CMD_HIDE_QR, .payload = nullptr, .param = 0 };
    if (xQueueSend(g_ui_queue, &m, 0) != pdTRUE) {
        return false;
    }
    return true;
}

