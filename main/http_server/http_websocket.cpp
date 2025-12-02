#include "esp_log.h"

#include "http_cors.h"
#include "http_utils.h"
#include "http_websocket.h"
#include "macros.h"

static const char* TAG = "http_websocket";

int websocket_fd = -1;

extern httpd_handle_t http_server;

QueueHandle_t log_queue = NULL;

void websocket_reset(void)
{
    // Restore normal logging
    esp_log_set_vprintf(vprintf);
    // Invalidate websocket websocket_fd
    websocket_fd = -1;
}

static int log_to_queue(const char * format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Calculate the required buffer size
    int needed_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    // Allocate the buffer dynamically
    char * log_buffer = (char *) CALLOC(needed_size + 2, sizeof(char));  // +2 for potential \n and \0
    if (log_buffer == NULL) {
        return 0;
    }

    // Format the string into the allocated buffer
    va_copy(args_copy, args);
    vsnprintf(log_buffer, needed_size, format, args_copy);
    va_end(args_copy);

    // Ensure the log message ends with a newline
    size_t len = strlen(log_buffer);
    if (len > 0 && log_buffer[len - 1] != '\n') {
        log_buffer[len] = '\n';
        log_buffer[len + 1] = '\0';
        len++;
    }

    // Print to standard output
    printf("%s", log_buffer);

	if (xQueueSendToBack(log_queue, (void*)&log_buffer, (TickType_t) 0) != pdPASS) {
		if (log_buffer != NULL) {
			FREE(log_buffer);
		}
	}
    return 0;
}

static void send_log_to_websocket(char *message)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = strlen(message);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if (http_server != NULL && websocket_fd >= 0) {
        esp_err_t err = httpd_ws_send_frame_async(http_server, websocket_fd, &ws_pkt);
        if (err != ESP_OK) {
            // Websocket is probably dead or socket reused
            websocket_reset();
        }
    }

    FREE(message);
}


/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
esp_err_t echo_handler(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        websocket_fd = httpd_req_to_sockfd(req);
        esp_log_set_vprintf(log_to_queue);
        return ESP_OK;
    }

    // Handle websocket frames (e.g. CLOSE)
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // We only need header info (opcode, len)
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket closed, disabling log forwarding");
        websocket_reset();
    }

    // We don't care about payload here
    return ESP_OK;
}


void websocket_log_handler(void* param)
{
    while (true)
    {
        char *message = NULL;

        if (xQueueReceive(log_queue, &message, (TickType_t) portMAX_DELAY) != pdPASS) {
            // message is either NULL or undefined, but we set it above
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (websocket_fd == -1) {
            // No active websocket, drop message
            if (message != NULL) {
                FREE(message);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        send_log_to_websocket(message);
    }
}



void websocket_start() {
    log_queue = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(char*));

    // Start websocket log handler thread
    xTaskCreate(&websocket_log_handler, "websocket_log_handler", 4096, NULL, 2, NULL);
}

