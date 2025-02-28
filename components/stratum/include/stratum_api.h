#pragma once

#include <cstddef>
#include <stdbool.h>
#include <stdint.h>
#include "ArduinoJson.h"

#define MAX_MERKLE_BRANCHES 32
#define HASH_SIZE 32
#define COINBASE_SIZE 100
#define COINBASE2_SIZE 128

typedef enum
{
    STRATUM_UNKNOWN,
    MINING_NOTIFY,
    MINING_SET_DIFFICULTY,
    MINING_SET_VERSION_MASK,
    STRATUM_RESULT,
    STRATUM_RESULT_SETUP,
    STRATUM_RESULT_VERSION_MASK,
    STRATUM_RESULT_SUBSCRIBE,
    CLIENT_RECONNECT
} stratum_method;

static const int STRATUM_ID_SUBSCRIBE = 1;
static const int STRATUM_ID_CONFIGURE = 2;

typedef struct
{
    char *job_id;
    uint8_t _prev_block_hash[HASH_SIZE];
    char *coinbase_1;
    char *coinbase_2;
    uint8_t _merkle_branches[MAX_MERKLE_BRANCHES][HASH_SIZE];
    size_t n_merkle_branches;
    uint32_t version;
    uint32_t version_mask;
    uint32_t target;
    uint32_t ntime;
    uint32_t difficulty;
} mining_notify;

typedef struct
{
    char *extranonce_str;
    int extranonce_2_len;

    int64_t message_id;
    // Indicates the type of request the message represents.
    stratum_method method;

    // mining.notify
    int should_abandon_work;
    mining_notify *mining_notification;
    // mining.set_difficulty
    uint32_t new_difficulty;
    // mining.set_version_mask
    uint32_t version_mask;
    // result
    bool response_success;
} StratumApiV1Message;

class StratumApi {
  private:
    // Fixed-size buffer used to accumulate partial incoming data.
    enum
    {
        BUFFER_SIZE = 1024,
        BIG_BUFFER_SIZE = 4096,
    };
    char *m_buffer;
    size_t m_len;   // Current length of valid data in m_buffer.
    int m_send_uid; // Message ID counter (each message gets a unique ID).

    // Helper: logs a transmit message (removing any trailing newline).
    void debugTx(const char *msg);

    // Helper functions for hex conversion.
    static uint8_t hex2val(char c);
    static size_t hex2bin(const char *hex, uint8_t *bin, size_t bin_len);

    // Helper: checks whether the socket is still connected.
    static int isSocketConnected(int socket);

    static bool parseMethods(JsonDocument &doc, const char* method_str, StratumApiV1Message *message);
    static bool parseResponses(JsonDocument &doc, StratumApiV1Message *message);

  public:
    StratumApi();
    ~StratumApi();

    // Receives a JSON-RPC line (terminated by '\n') from the socket.
    // Returns a dynamically allocated C-string that the caller must free.
    char *receiveJsonRpcLine(int sockfd);

    // Sends a subscribe message.
    int subscribe(int socket, const char *device, const char *asic);

    // Sends a suggest-difficulty message.
    int suggestDifficulty(int socket, uint32_t difficulty);

    // Sends an authentication message.
    int authenticate(int socket, const char *username, const char *pass);

    // Submits a share.
    void submitShare(int socket, const char *username, const char *jobid, const char *extranonce_2, uint32_t ntime, uint32_t nonce,
                     uint32_t version);

    // Sends a configure-version-rolling message.
    void configureVersionRolling(int socket);

    // Resets the message ID counter.
    void resetUid();

    // clear the message buffer
    void clearBuffer();

    // Parses a received JSON string into a StratumApiV1Message.
    static bool parse(StratumApiV1Message* message, const char* stratum_json);
    static bool parse(StratumApiV1Message *message, JsonDocument &doc);

    // Frees a mining_notify structure allocated in parse().
    static void freeMiningNotify(mining_notify *params);

};
