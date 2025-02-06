/******************************************************************************
 *  *
 * References:
 *  1. Stratum Protocol - https://reference.cash/mining/stratum-protocol
 *****************************************************************************/

#include "stratum_api.h"      // Assumes that types like StratumApiV1Message,
                              // mining_notify, STRATUM_ID_SUBSCRIBE, etc., are defined here.
#include "cJSON.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

// The logging tag for ESP logging.
static const char *TAG = "stratum_api";

#ifdef CONFIG_SPIRAM
#define ALLOC(s)    heap_caps_malloc(s, MALLOC_CAP_SPIRAM)
#else
#define ALLOC(s)    malloc(s)
#endif

StratumApi::StratumApi() : m_len(0), m_send_uid(1)
{
    m_buffer = (char *) ALLOC(BIG_BUFFER_SIZE);
    memset(m_buffer, 0, BIG_BUFFER_SIZE);
}

StratumApi::~StratumApi()
{
    // Nothing to free.
}

uint8_t StratumApi::hex2val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return 0;
    }
}

size_t StratumApi::hex2bin(const char* hex, uint8_t* bin, size_t bin_len)
{
    size_t len = 0;
    while (*hex && len < bin_len) {
        bin[len] = hex2val(*hex++) << 4;
        if (!*hex) {
            len++;
            break;
        }
        bin[len++] |= hex2val(*hex++);
    }
    return len;
}

int StratumApi::isSocketConnected(int socket)
{
    if (socket == -1) {
        return 0;
    }
    struct timeval tv;
    fd_set writefds;

    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms timeout

    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);

    int ret = select(socket + 1, NULL, &writefds, NULL, &tv);
    return (ret > 0 && FD_ISSET(socket, &writefds)) ? 1 : 0;
}

void StratumApi::debugTx(const char* msg)
{
    const char* newline = strchr(msg, '\n');
    if (newline != NULL) {
        ESP_LOGI(TAG, "tx: %.*s", (int)(newline - msg), msg);
    } else {
        ESP_LOGI(TAG, "tx: %s", msg);
    }
}

//--------------------------------------------------------------------
// receiveJsonRpcLine()
//--------------------------------------------------------------------
// Accumulates data from the given socket until a newline is found.
// Returns a dynamically allocated line (caller must free the returned memory).
//--------------------------------------------------------------------
char* StratumApi::receiveJsonRpcLine(int sockfd)
{
    while (strchr(m_buffer, '\n') == NULL)
    {
        if (m_len >= BIG_BUFFER_SIZE - 1) {
            ESP_LOGE(TAG, "Buffer full without newline. Flushing buffer.");
            m_len = 0;
            m_buffer[0] = '\0';
        }
        int available = BIG_BUFFER_SIZE - m_len - 1; // reserve space for terminating null
        int nbytes = recv(sockfd, m_buffer + m_len, available, 0);
        if (nbytes == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ESP_LOGW(TAG, "No transmission from Stratum server. Checking socket ...");
                if (isSocketConnected(sockfd)) {
                    ESP_LOGI(TAG, "Socket is still connected.");
                    continue;  // Retry recv() until data arrives.
                } else {
                    ESP_LOGE(TAG, "Socket is not connected anymore.");
                    m_len = 0;
                    m_buffer[0] = '\0';
                    return NULL;
                }
            } else {
                ESP_LOGE(TAG, "Error in recv: %s", strerror(errno));
                m_len = 0;
                m_buffer[0] = '\0';
                return NULL;
            }
        }
        else if (nbytes == 0) {
            // Remote end closed the connection.
            return NULL;
        }
        m_len += nbytes;
        m_buffer[m_len] = '\0';
    }

    // At this point, the buffer contains at least one newline.
    char* newline_ptr = strchr(m_buffer, '\n');
    int line_length = newline_ptr - m_buffer;

    // Allocate memory for the resulting line.
    char* line = (char*)ALLOC(line_length + 1);

    if (!line) {
        ESP_LOGE(TAG, "Failed to allocate memory for line.");
        return NULL;
    }
    memcpy(line, m_buffer, line_length);
    line[line_length] = '\0';

    // Remove the extracted line (including the newline) from the buffer.
    int remaining = m_len - (line_length + 1);
    if (remaining > 0) {
        memmove(m_buffer, newline_ptr + 1, remaining);
    }
    m_len = remaining;
    m_buffer[m_len] = '\0';

    return line;
}

//--------------------------------------------------------------------
// parse()
//--------------------------------------------------------------------
// Parses a given JSON string into a StratumApiV1Message.
//--------------------------------------------------------------------
void StratumApi::parse(StratumApiV1Message* message, const char* stratum_json)
{
    cJSON* json = cJSON_Parse(stratum_json);
    cJSON* id_json = cJSON_GetObjectItem(json, "id");
    int64_t parsed_id = -1;
    if (id_json != NULL && cJSON_IsNumber(id_json)) {
        parsed_id = id_json->valueint;
    }
    message->message_id = parsed_id;

    cJSON* method_json = cJSON_GetObjectItem(json, "method");
    stratum_method result = STRATUM_UNKNOWN;

    if (method_json != NULL && cJSON_IsString(method_json)) {
        if (strcmp("mining.notify", method_json->valuestring) == 0) {
            result = MINING_NOTIFY;
        } else if (strcmp("mining.set_difficulty", method_json->valuestring) == 0) {
            result = MINING_SET_DIFFICULTY;
        } else if (strcmp("mining.set_version_mask", method_json->valuestring) == 0) {
            result = MINING_SET_VERSION_MASK;
        } else if (strcmp("client.reconnect", method_json->valuestring) == 0) {
            result = CLIENT_RECONNECT;
        } else {
            ESP_LOGI(TAG, "unhandled method in stratum message: %s", stratum_json);
        }
    } else {
        cJSON* result_json = cJSON_GetObjectItem(json, "result");
        cJSON* error_json = cJSON_GetObjectItem(json, "error");

        if (result_json == NULL) {
            message->response_success = false;
        } else if (!cJSON_IsNull(error_json)) {
            if (parsed_id < 5) {
                result = STRATUM_RESULT_SETUP;
            } else {
                result = STRATUM_RESULT;
            }
            message->response_success = false;
        } else if (cJSON_IsBool(result_json)) {
            if (parsed_id < 5) {
                result = STRATUM_RESULT_SETUP;
            } else {
                result = STRATUM_RESULT;
            }
            message->response_success = cJSON_IsTrue(result_json) ? true : false;
        } else if (parsed_id == STRATUM_ID_SUBSCRIBE) {
            result = STRATUM_RESULT_SUBSCRIBE;

            cJSON* extranonce2_len_json = cJSON_GetArrayItem(result_json, 2);
            if (extranonce2_len_json == NULL) {
                ESP_LOGE(TAG, "Unable to parse extranonce2_len: %s", result_json->valuestring);
                message->response_success = false;
                goto done;
            }
            message->extranonce_2_len = extranonce2_len_json->valueint;

            cJSON* extranonce_json = cJSON_GetArrayItem(result_json, 1);
            if (extranonce_json == NULL) {
                ESP_LOGE(TAG, "Unable parse extranonce: %s", result_json->valuestring);
                message->response_success = false;
                goto done;
            }
            message->extranonce_str = (char*)malloc(strlen(extranonce_json->valuestring) + 1);
            strcpy(message->extranonce_str, extranonce_json->valuestring);
            message->response_success = true;

            ESP_LOGI(TAG, "extranonce_str: %s", message->extranonce_str);
            ESP_LOGI(TAG, "extranonce_2_len: %d", message->extranonce_2_len);
        } else if (parsed_id == STRATUM_ID_CONFIGURE) {
            cJSON* mask = cJSON_GetObjectItem(result_json, "version-rolling.mask");
            if (mask != NULL) {
                result = STRATUM_RESULT_VERSION_MASK;
                message->version_mask = strtoul(mask->valuestring, NULL, 16);
                ESP_LOGI(TAG, "Set version mask: %08lx", message->version_mask);
            } else {
                ESP_LOGI(TAG, "error setting version mask: %s", stratum_json);
            }
        } else {
            ESP_LOGI(TAG, "unhandled result in stratum message: %s", stratum_json);
        }
    }

    message->method = result;

    if (message->method == MINING_NOTIFY) {
        mining_notify* new_work = (mining_notify*)ALLOC(sizeof(mining_notify));

        cJSON* params = cJSON_GetObjectItem(json, "params");

        new_work->job_id = strdup(cJSON_GetArrayItem(params, 0)->valuestring);

        hex2bin(cJSON_GetArrayItem(params, 1)->valuestring, new_work->_prev_block_hash, HASH_SIZE);

        new_work->coinbase_1 = strdup(cJSON_GetArrayItem(params, 2)->valuestring);
        new_work->coinbase_2 = strdup(cJSON_GetArrayItem(params, 3)->valuestring);

        cJSON* merkle_branch = cJSON_GetArrayItem(params, 4);
        new_work->n_merkle_branches = cJSON_GetArraySize(merkle_branch);
        if (new_work->n_merkle_branches > MAX_MERKLE_BRANCHES) {
            printf("Too many Merkle branches.\n");
            abort();
        }

        for (size_t i = 0; i < new_work->n_merkle_branches; i++) {
            hex2bin(cJSON_GetArrayItem(merkle_branch, i)->valuestring, new_work->_merkle_branches[i], HASH_SIZE);
        }

        new_work->version = strtoul(cJSON_GetArrayItem(params, 5)->valuestring, NULL, 16);
        new_work->target = strtoul(cJSON_GetArrayItem(params, 6)->valuestring, NULL, 16);
        new_work->ntime = strtoul(cJSON_GetArrayItem(params, 7)->valuestring, NULL, 16);

        message->mining_notification = new_work;

        int paramsLength = cJSON_GetArraySize(params);
        int value = cJSON_IsTrue(cJSON_GetArrayItem(params, paramsLength - 1));
        message->should_abandon_work = value;
    }
    else if (message->method == MINING_SET_DIFFICULTY) {
        cJSON* params = cJSON_GetObjectItem(json, "params");
        uint32_t difficulty = cJSON_GetArrayItem(params, 0)->valueint;
        message->new_difficulty = difficulty;
    }
    else if (message->method == MINING_SET_VERSION_MASK) {
        cJSON* params = cJSON_GetObjectItem(json, "params");
        uint32_t version_mask = strtoul(cJSON_GetArrayItem(params, 0)->valuestring, NULL, 16);
        message->version_mask = version_mask;
    }
done:
    cJSON_Delete(json);
}

//--------------------------------------------------------------------
// freeMiningNotify()
//--------------------------------------------------------------------
void StratumApi::freeMiningNotify(mining_notify* params)
{
    free(params->job_id);
    free(params->coinbase_1);
    free(params->coinbase_2);
    free(params);
}

//--------------------------------------------------------------------
// parseSubscribeResultMessage()
//--------------------------------------------------------------------
int StratumApi::parseSubscribeResultMessage(const char *result_json_str, char **extranonce, int *extranonce2_len)
{
    cJSON* root = cJSON_Parse(result_json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Unable to parse %s", result_json_str);
        return -1;
    }
    cJSON* result = cJSON_GetObjectItem(root, "result");
    if (result == NULL) {
        ESP_LOGE(TAG, "Unable to parse subscribe result %s", result_json_str);
        cJSON_Delete(root);
        return -1;
    }

    cJSON* extranonce2_len_json = cJSON_GetArrayItem(result, 2);
    if (extranonce2_len_json == NULL) {
        ESP_LOGE(TAG, "Unable to parse extranonce2_len: %s", result->valuestring);
        cJSON_Delete(root);
        return -1;
    }
    *extranonce2_len = extranonce2_len_json->valueint;

    cJSON* extranonce_json = cJSON_GetArrayItem(result, 1);
    if (extranonce_json == NULL) {
        ESP_LOGE(TAG, "Unable parse extranonce: %s", result->valuestring);
        cJSON_Delete(root);
        return -1;
    }
    *extranonce = (char*)malloc(strlen(extranonce_json->valuestring) + 1);
    strcpy(*extranonce, extranonce_json->valuestring);

    cJSON_Delete(root);
    return 0;
}

//--------------------------------------------------------------------
// subscribe()
//--------------------------------------------------------------------
int StratumApi::subscribe(int socket, const char* device, const char* asic)
{
    char subscribe_msg[BUFFER_SIZE];
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    const char *version = app_desc->version;
    sprintf(subscribe_msg,
            "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": [\"%s/%s/%s\"]}\n",
            m_send_uid++, device, asic, version);
    debugTx(subscribe_msg);
    write(socket, subscribe_msg, strlen(subscribe_msg));
    return 1;
}

//--------------------------------------------------------------------
// suggestDifficulty()
//--------------------------------------------------------------------
int StratumApi::suggestDifficulty(int socket, uint32_t difficulty)
{
    char difficulty_msg[BUFFER_SIZE];
    sprintf(difficulty_msg,
            "{\"id\": %d, \"method\": \"mining.suggest_difficulty\", \"params\": [%ld]}\n",
            m_send_uid++, difficulty);
    debugTx(difficulty_msg);
    write(socket, difficulty_msg, strlen(difficulty_msg));
    return 1;
}

//--------------------------------------------------------------------
// authenticate()
//--------------------------------------------------------------------
int StratumApi::authenticate(int socket, const char* username, const char* pass)
{
    char authorize_msg[BUFFER_SIZE];
    sprintf(authorize_msg,
            "{\"id\": %d, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}\n",
            m_send_uid++, username, pass);
    debugTx(authorize_msg);
    write(socket, authorize_msg, strlen(authorize_msg));
    return 1;
}

//--------------------------------------------------------------------
// submitShare()
//--------------------------------------------------------------------
void StratumApi::submitShare(int socket, const char* username, const char* jobid,
                             const char* extranonce_2, uint32_t ntime, uint32_t nonce, uint32_t version)
{
    char submit_msg[BUFFER_SIZE];
    if (!isSocketConnected(socket)) {
        ESP_LOGI(TAG, "Socket not connected. Cannot send message.");
        return;
    }
    sprintf(submit_msg,
            "{\"id\": %d, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%08lx\", \"%08lx\", \"%08lx\"]}\n",
            m_send_uid++, username, jobid, extranonce_2, ntime, nonce, version);
    debugTx(submit_msg);
    ssize_t bytes_written = write(socket, submit_msg, strlen(submit_msg));
    if (bytes_written == -1) {
        ESP_LOGE(TAG, "Error writing to socket: %s", strerror(errno));
    }
}

//--------------------------------------------------------------------
// configureVersionRolling()
//--------------------------------------------------------------------
void StratumApi::configureVersionRolling(int socket)
{
    char configure_msg[BUFFER_SIZE * 2];
    sprintf(configure_msg,
            "{\"id\": %d, \"method\": \"mining.configure\", \"params\": [[\"version-rolling\"], {\"version-rolling.mask\": \"ffffffff\"}]}\n",
            m_send_uid++);
    debugTx(configure_msg);
    write(socket, configure_msg, strlen(configure_msg));
}

//--------------------------------------------------------------------
// resetUid()
//--------------------------------------------------------------------
void StratumApi::resetUid()
{
    ESP_LOGI(TAG, "Resetting stratum uid");
    m_send_uid = 1;
}
