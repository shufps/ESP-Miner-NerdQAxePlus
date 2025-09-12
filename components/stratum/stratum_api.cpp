/******************************************************************************
 *  *
 * References:
 *  1. Stratum Protocol - https://reference.cash/mining/stratum-protocol
 *****************************************************************************/

#include <cstddef>

#include "stratum_api.h" // Assumes that types like StratumApiV1Message,
                         // mining_notify, STRATUM_ID_SUBSCRIBE, etc., are defined here.
#include "ArduinoJson.h"
#include "psram_allocator.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

// The logging tag for ESP logging.
static const char *TAG = "stratum_api";

void safe_free(char *&ptr)
{
    if (ptr) {         // Check if pointer is not null
        free(ptr);     // Free memory
        ptr = nullptr; // Set pointer to null to prevent dangling pointer issues
    }
}

StratumApi::StratumApi() : m_len(0), m_send_uid(1)
{
    m_buffer = (char *) MALLOC(BIG_BUFFER_SIZE);
    m_requestBuffer = (char *) MALLOC(BUFFER_SIZE);
    clearBuffer();
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

size_t StratumApi::hex2bin(const char *hex, uint8_t *bin, size_t bin_len)
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

void StratumApi::debugTx(const char *msg)
{
    const char *newline = strchr(msg, '\n');
    if (newline != NULL) {
        ESP_LOGI(TAG, "tx: %.*s", (int) (newline - msg), msg);
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
char *StratumApi::receiveJsonRpcLine(int sockfd)
{
    while (strchr(m_buffer, '\n') == NULL) {
        if (m_len >= BIG_BUFFER_SIZE - 1) {
            ESP_LOGE(TAG, "Buffer full without newline. Flushing buffer.");
            m_len = 0;
            m_buffer[0] = '\0';
        }
        int available = BIG_BUFFER_SIZE - m_len - 1; // reserve space for terminating null
        int nbytes = recv(sockfd, m_buffer + m_len, available, 0);
        if (nbytes == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ESP_LOGI(TAG, "No transmission from Stratum server. Checking socket ...");
                if (isSocketConnected(sockfd)) {
                    ESP_LOGI(TAG, "Socket is still connected.");
                    continue; // Retry recv() until data arrives.
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
        } else if (nbytes == 0) {
            // Remote end closed the connection.
            return NULL;
        }
        m_len += nbytes;
        m_buffer[m_len] = '\0';
    }

    // At this point, the buffer contains at least one newline.
    char *newline_ptr = strchr(m_buffer, '\n');
    int line_length = newline_ptr - m_buffer;

    // Allocate memory for the resulting line.
    char *line = (char *) MALLOC(line_length + 1);

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

bool StratumApi::parseMethods(JsonDocument &doc, const char *method_str, StratumApiV1Message *message)
{
    message->method = STRATUM_UNKNOWN;

    if (strcmp(method_str, "mining.notify") == 0) {
        message->method = MINING_NOTIFY;
    } else if (strcmp(method_str, "mining.set_difficulty") == 0) {
        message->method = MINING_SET_DIFFICULTY;
    } else if (strcmp(method_str, "mining.set_version_mask") == 0) {
        message->method = MINING_SET_VERSION_MASK;
    } else if (strcmp(method_str, "client.reconnect") == 0) {
        message->method = CLIENT_RECONNECT;
    } else {
        ESP_LOGI(TAG, "Unhandled method in stratum message: %s", method_str);
        return false;
    }

    switch (message->method) {
    case MINING_NOTIFY: {
        ESP_LOGI(TAG, "mining notify");
        mining_notify *new_work = (mining_notify *) MALLOC(sizeof(mining_notify));

        JsonArray params = doc["params"].as<JsonArray>();

        new_work->job_id = strdup(params[0].as<const char *>());
        hex2bin(params[1].as<const char *>(), new_work->_prev_block_hash, HASH_SIZE);

        new_work->coinbase_1 = strdup(params[2].as<const char *>());
        new_work->coinbase_2 = strdup(params[3].as<const char *>());

        JsonArray merkle_branch = params[4].as<JsonArray>();
        new_work->n_merkle_branches = merkle_branch.size();
        if (new_work->n_merkle_branches > MAX_MERKLE_BRANCHES) {
            ESP_LOGE(TAG, "Too many Merkle branches.");
            return false;
        }

        for (size_t i = 0; i < new_work->n_merkle_branches; i++) {
            hex2bin(merkle_branch[i].as<const char *>(), new_work->_merkle_branches[i], HASH_SIZE);
        }

        new_work->version = strtoul(params[5].as<const char *>(), NULL, 16);
        new_work->target = strtoul(params[6].as<const char *>(), NULL, 16);
        new_work->ntime = strtoul(params[7].as<const char *>(), NULL, 16);

        message->mining_notification = new_work;

        int paramsLength = params.size();
        message->should_abandon_work = params[paramsLength - 1].as<bool>();
        break;
    }
    case MINING_SET_DIFFICULTY:
        message->new_difficulty = doc["params"][0].as<uint32_t>();
        break;
    case MINING_SET_VERSION_MASK:
        message->version_mask = strtoul(doc["params"][0].as<const char *>(), NULL, 16);
        break;
    default:
        break;
    }

    // ESP_LOGI(TAG, "allocs: %d, deallocs: %d, reallocs: %d", allocs, deallocs, reallocs);
    return true;
}

bool StratumApi::parseResult(JsonDocument &doc) {
    JsonVariant result_json = doc["result"];
    JsonVariant error_json = doc["error"];

    if (!error_json.isNull()) {
        return false;
    }

    if (!result_json.isNull()) {
        return result_json.is<bool>() ? result_json.as<bool>() : false;
    }

    return false;
}

bool StratumApi::parseResponses(JsonDocument &doc, StratumApiV1Message *message)
{
    message->method = STRATUM_RESULT;
    message->response_success = parseResult(doc);
    return true;
}

bool StratumApi::parseSetupResponses(JsonDocument &doc, StratumApiV1Message *message)
{
    // first messages are responses to our mining setup requests
    message->method = STRATUM_UNKNOWN;

    JsonVariant result_json = doc["result"];

    switch (message->message_id) {
    case STRATUM_ID_SUBSCRIBE: {
        message->method = STRATUM_RESULT_SUBSCRIBE;

        JsonArray result_arr = result_json.as<JsonArray>();
        if (result_arr.size() < 3) {
            ESP_LOGE(TAG, "Invalid result array for subscribe.");
            return false;
        }
        message->extranonce_2_len = result_arr[2].as<int>();

        const char *extranonce_str = result_arr[1].as<const char *>();
        if (!extranonce_str) {
            ESP_LOGE(TAG, "extranonce is null");
            return false;
        }
        message->extranonce_str = strdup(extranonce_str);

        ESP_LOGI(TAG, "extranonce_str: %s", message->extranonce_str);
        ESP_LOGI(TAG, "extranonce_2_len: %d", message->extranonce_2_len);
        break;
    }
    case STRATUM_ID_CONFIGURE: {
        message->method = STRATUM_RESULT_VERSION_MASK;

        const char *mask = result_json["version-rolling.mask"].as<const char *>();
        if (!mask) {
            return false;
        }
        message->version_mask = strtoul(mask, NULL, 16);
        ESP_LOGI(TAG, "Set version mask: %08lx", message->version_mask);
        break;
    }
    case STRATUM_ID_AUTHORIZE: {
        message->method = STRATUM_RESULT_SETUP;
        message->response_success = parseResult(doc);
        break;
    }
    case STRATUM_ID_SUGGEST_DIFFICULTY: {
        message->method = STRATUM_RESULT_SETUP;
        message->response_success = parseResult(doc);
        break;
    }
    default:
        ESP_LOGW(TAG, "unhandled ID");
        return false;
    }
    return true;
}

bool StratumApi::parse(StratumApiV1Message *message, const char *stratum_json)
{
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Deserialize JSON
    DeserializationError error = deserializeJson(doc, stratum_json);
    if (error) {
        ESP_LOGE(TAG, "Unable to parse JSON: %s", error.c_str());
        return false;
    }

    return parse(message, doc);
}

bool StratumApi::parse(StratumApiV1Message *message, JsonDocument &doc)
{
    // Extract message ID
    message->message_id = doc["id"].is<int>() ? doc["id"].as<int>() : -1;

    // Extract method
    const char *method_str = doc["method"].as<const char *>();

    if (method_str) {
        return parseMethods(doc, method_str, message);
    } else {
        if (message->message_id < 5) {
            return parseSetupResponses(doc, message);
        }
        return parseResponses(doc, message);
    }
}

//--------------------------------------------------------------------
// freeMiningNotify()
//--------------------------------------------------------------------
void StratumApi::freeMiningNotify(mining_notify *params)
{
    safe_free(params->job_id);
    safe_free(params->coinbase_1);
    safe_free(params->coinbase_2);
}

//--------------------------------------------------------------------
// send()
//--------------------------------------------------------------------
bool StratumApi::send(int socket, const char *message)
{
    debugTx(message);

    if (!isSocketConnected(socket)) {
        ESP_LOGI(TAG, "Socket not connected. Cannot send message.");
        return false;
    }

    ssize_t bytes_written = write(socket, message, strlen(message));
    if (bytes_written == -1) {
        ESP_LOGE(TAG, "Error writing to socket: %s", strerror(errno));
        return false;
    }
    return true;
}

//--------------------------------------------------------------------
// subscribe()
//--------------------------------------------------------------------
bool StratumApi::subscribe(int socket, const char *device, const char *asic)
{
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    const char *version = app_desc->version;
    snprintf(m_requestBuffer, BUFFER_SIZE, "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": [\"%s/%s/%s\"]}\n",
             m_send_uid++, device, asic, version);

    return send(socket, m_requestBuffer);
}

//--------------------------------------------------------------------
// suggestDifficulty()
//--------------------------------------------------------------------
bool StratumApi::suggestDifficulty(int socket, uint32_t difficulty)
{
    snprintf(m_requestBuffer, BUFFER_SIZE, "{\"id\": %d, \"method\": \"mining.suggest_difficulty\", \"params\": [%ld]}\n",
             m_send_uid++, difficulty);

    return send(socket, m_requestBuffer);
}

//--------------------------------------------------------------------
// authenticate()
//--------------------------------------------------------------------
bool StratumApi::authenticate(int socket, const char *username, const char *pass)
{
    snprintf(m_requestBuffer, BUFFER_SIZE, "{\"id\": %d, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}\n",
             m_send_uid++, username, pass);

    return send(socket, m_requestBuffer);
}

//--------------------------------------------------------------------
// submitShare()
//--------------------------------------------------------------------
bool StratumApi::submitShare(int socket, const char *username, const char *jobid, const char *extranonce_2, uint32_t ntime,
                             uint32_t nonce, uint32_t version)
{
    snprintf(m_requestBuffer, BUFFER_SIZE,
             "{\"id\": %d, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%08lx\", \"%08lx\", \"%08lx\"]}\n",
             m_send_uid++, username, jobid, extranonce_2, ntime, nonce, version);

    return send(socket, m_requestBuffer);
}

//--------------------------------------------------------------------
// configureVersionRolling()
//--------------------------------------------------------------------
bool StratumApi::configureVersionRolling(int socket)
{
    snprintf(m_requestBuffer, BUFFER_SIZE,
             "{\"id\": %d, \"method\": \"mining.configure\", \"params\": [[\"version-rolling\"], {\"version-rolling.mask\": "
             "\"1fffe000\"}]}\n",
             m_send_uid++);

    return send(socket, m_requestBuffer);
}

//--------------------------------------------------------------------
// resetUid()
//--------------------------------------------------------------------
void StratumApi::resetUid()
{
    ESP_LOGI(TAG, "Resetting stratum uid");
    m_send_uid = 1;
}

//--------------------------------------------------------------------
// clearBuffer()
//--------------------------------------------------------------------
void StratumApi::clearBuffer()
{
    memset(m_buffer, 0, BIG_BUFFER_SIZE);
    m_len = 0;
}
