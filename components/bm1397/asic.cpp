#include <string.h>
#include <math.h>
#include <endian.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "utils.h"
#include "serial.h"
#include "asic.h"
#include "crc.h"

typedef enum
{
    JOB_PACKET = 0,
    CMD_PACKET = 1,
} packet_type_t;


typedef struct __attribute__((__packed__))
{
    uint8_t job_id;
    uint8_t num_midstates;
    uint8_t starting_nonce[4];
    uint8_t nbits[4];
    uint8_t ntime[4];
    uint8_t merkle_root[32];
    uint8_t prev_block_hash[32];
    uint8_t version[4];
} BM1368_job;

const static char* TAG = "asic";

Asic::Asic(uint8_t asicCount) : 
   m_current_frequency{ 56.25 },
   m_asic_count{ asicCount }
{
   m_vCurrentFrequencies.resize(m_asic_count);
   for (uint8_t i = 0; i < m_asic_count; ++i) {
       m_vCurrentFrequencies[i] = m_current_frequency;
   }
}

uint16_t Asic::reverseUint16(uint16_t num)
{
    return (num >> 8) | (num << 8);
}

void Asic::send(uint8_t header, uint8_t *data, uint8_t data_len, bool debug)
{
    packet_type_t packet_type = (header & TYPE_JOB) ? JOB_PACKET : CMD_PACKET;
    uint8_t total_length = (packet_type == JOB_PACKET) ? (data_len + 6) : (data_len + 5);

    unsigned char buf[total_length];

    // add the preamble
    buf[0] = 0x55;
    buf[1] = 0xAA;

    // add the header field
    buf[2] = header;

    // add the length field
    buf[3] = (packet_type == JOB_PACKET) ? (data_len + 4) : (data_len + 3);

    // add the data
    memcpy(buf + 4, data, data_len);

    // add the correct crc type
    if (packet_type == JOB_PACKET) {
        uint16_t crc16_total = crc16_false(buf + 2, data_len + 2);
        buf[4 + data_len] = (crc16_total >> 8) & 0xFF;
        buf[5 + data_len] = crc16_total & 0xFF;
    } else {
        buf[4 + data_len] = crc5(buf + 2, data_len + 2);
    }

    // send serial data
    SERIAL_send(buf, total_length, debug);
}

void Asic::send6(uint8_t header, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5) {
    uint8_t buf[6] = {b0, b1, b2, b3, b4, b5};
    send(header, buf, sizeof(buf), ASIC_SERIALTX_DEBUG);
}

void Asic::send2(uint8_t header, uint8_t b0, uint8_t b1) {
    uint8_t buf[2] = {b0, b1};
    send(header, buf, sizeof(buf), ASIC_SERIALTX_DEBUG);
}

void Asic::sendChainInactive(void)
{
    send2(TYPE_CMD | GROUP_ALL | CMD_INACTIVE, 0x00, 0x00);
}

void Asic::setChipAddress(uint8_t chipAddr)
{
    send2(TYPE_CMD | GROUP_SINGLE | CMD_SETADDRESS, chipAddr, 0x00);
}

void Asic::sendReadAddress(void)
{
    send2(TYPE_CMD | GROUP_ALL | CMD_READ, 0x00, 0x00);
}

std::optional<std::array<uint8_t, 6>> Asic::getFreqBuf(uint8_t asic_index, float target_freq, float& best_newf) const {
    const float max_diff = 0.001;

    std::array<uint8_t, 6> freqbuf = {0x00, 0x08, 0x40, 0xA0, 0x02, 0x41};
    int postdiv_min = 255;
    int postdiv2_min = 255;
    int refdiv, fb_divider, postdiv1, postdiv2;
    float newf;
    bool found = false;
    int best_refdiv, best_fb_divider, best_postdiv1, best_postdiv2;

    for (refdiv = 2; refdiv > 0; refdiv--) {
        for (postdiv1 = 7; postdiv1 > 0; postdiv1--) {
            for (postdiv2 = 7; postdiv2 > 0; postdiv2--) {
                fb_divider = (int)round(target_freq / 25.0 * (refdiv * postdiv2 * postdiv1));
                newf = 25.0 * fb_divider / (refdiv * postdiv2 * postdiv1);
                if (
                    fb_divider >= 0xa0 && fb_divider <= 0xef &&
                    fabs(target_freq - newf) < max_diff &&
                    postdiv1 >= postdiv2 &&
                    postdiv1 * postdiv2 < postdiv_min &&
                    postdiv2 <= postdiv2_min
                ) {
                    postdiv2_min = postdiv2;
                    postdiv_min = postdiv1 * postdiv2;
                    best_refdiv = refdiv;
                    best_fb_divider = fb_divider;
                    best_postdiv1 = postdiv1;
                    best_postdiv2 = postdiv2;
                    best_newf = newf;
                    found = true;
                }
            }
        }
    }

    if (!found) {
        ESP_LOGE(TAG, "Didn't find PLL settings for target frequency %.2f", target_freq);
        return std::nullopt;
    }

    freqbuf[2] = (best_fb_divider * 25 / best_refdiv >= 2400) ? 0x50 : 0x40;
    freqbuf[3] = best_fb_divider;
    freqbuf[4] = best_refdiv;
    freqbuf[5] = (((best_postdiv1 - 1) & 0xf) << 4) | ((best_postdiv2 - 1) & 0xf);

    if (asic_index != std::numeric_limits<uint8_t>::max()) {
       freqbuf[0] = asicIndexToChipAddress(asic_index);
    }

    return freqbuf;
}

bool Asic::sendHashFrequency(uint8_t asic_index, float target_freq) {

    if (asic_index < std::numeric_limits<uint8_t>::max() && asic_index >= m_asic_count) {
        ESP_LOGE(TAG, "Invalid ASIC index %d, max is %d", asic_index, m_asic_count - 1);
        return false;
    }

    float best_newf;
    auto freqbuf = getFreqBuf(asic_index, target_freq, best_newf);
    if (!freqbuf.has_value()) {
        ESP_LOGE(TAG, "Failed to get frequency buffer for target frequency %.2f", target_freq);
        return false;
    }

    if (asic_index == std::numeric_limits<uint8_t>::max()) {
       send(CMD_WRITE_ALL, freqbuf->data(), sizeof(*freqbuf), ASIC_SERIALTX_DEBUG);
       //ESP_LOG_BUFFER_HEX(TAG, freqbuf, sizeof(freqbuf));

       ESP_LOGI(TAG, "Setting Frequency %d to %.2fMHz (%.2f)", asic_index, target_freq, best_newf);
       m_current_frequency = target_freq;
       for (uint8_t i = 0; i < m_asic_count; ++i) {
          m_vCurrentFrequencies[i] = target_freq;
       }
    } else {
       send(CMD_WRITE_SINGLE, freqbuf->data(), sizeof(*freqbuf), ASIC_SERIALTX_DEBUG);
       //ESP_LOG_BUFFER_HEX(TAG, freqbuf, sizeof(freqbuf));

       ESP_LOGI(TAG, "Setting Frequency of ASIC %d to %.2fMHz (%.2f)", asic_index, target_freq, best_newf);
       m_vCurrentFrequencies[asic_index] = target_freq;
    }
    return true;
}

// Function to perform frequency transition up or down
bool Asic::doFrequencyTransition(uint8_t asic_index, float target_frequency) {
    float step = 6.25;
    float current = m_current_frequency;
    float target = target_frequency;

    // Determine the direction of the transition
    float direction = (target > current) ? step : -step;

    // Align to the next 6.25-dividable value if not already on one
    if (fmod(current, step) != 0) {
        // If ramping up, round up to the next multiple; if ramping down, round down
        float next_dividable;
        if (direction > 0) {
            next_dividable = ceil(current / step) * step;
        } else {
            next_dividable = floor(current / step) * step;
        }
        current = next_dividable;
         if (!sendHashFrequency(asic_index, current)) {
             printf("ERROR: Failed to set frequency of ASIC #%d to %.2f MHz\n", asic_index, current);
             return false;
         }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Ramp in the appropriate direction
    while ((direction > 0 && current < target) || (direction < 0 && current > target)) {
        float next_step = fmin(fabs(direction), fabs(target - current));
        current += direction > 0 ? next_step : -next_step;
         if (!sendHashFrequency(asic_index, current)) {
             printf("ERROR: Failed to set frequency of ASIC #%d to %.2f MHz\n", asic_index, current);
             return false;
         }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Set the exact target frequency to finalize
     if (!sendHashFrequency(asic_index, target)) {
         printf("ERROR: Failed to set frequency of ASIC #%d to %.2f MHz\n", asic_index, target);
         return false;
     }
    return true;
}

int Asic::count_asics() {

    // read register 00 on all chips (should respond AA 55 13 68 00 00 00 00 00 00 0F)
    send2(CMD_READ_ALL, 0x00, 0x00);

    uint8_t buf[11];
    int chip_counter = 0;
    while (SERIAL_rx(buf, sizeof(buf), 1000) > 0) {
//        ESP_LOG_BUFFER_HEX(TAG, buf, sizeof(buf));
        if (!strncmp((char *) getChipId(), (char *) buf, 6)) {
            chip_counter++;
            ESP_LOGI(TAG, "found asic #%d", chip_counter);
        } else {
            ESP_LOGE(TAG, "unexpected response ... ignoring ...");
            ESP_LOG_BUFFER_HEX(TAG, buf, sizeof(buf));
        }
    }
    return chip_counter;
}

void Asic::setJobDifficultyMask(int difficulty)
{
    // Default mask of 256 diff
    unsigned char job_difficulty_mask[9] = {0x00, TICKET_MASK, 0b00000000, 0b00000000, 0b00000000, 0b11111111};

    // The mask must be a power of 2 so there are no holes
    // Correct:  {0b00000000, 0b00000000, 0b11111111, 0b11111111}
    // Incorrect: {0b00000000, 0b00000000, 0b11100111, 0b11111111}
    // (difficulty - 1) if it is a pow 2 then step down to second largest for more hashrate sampling
    difficulty = _largest_power_of_two(difficulty) - 1;

    // convert difficulty into char array
    // Ex: 256 = {0b00000000, 0b00000000, 0b00000000, 0b11111111}, {0x00, 0x00, 0x00, 0xff}
    // Ex: 512 = {0b00000000, 0b00000000, 0b00000001, 0b11111111}, {0x00, 0x00, 0x01, 0xff}
    for (int i = 0; i < 4; i++) {
        char value = (difficulty >> (8 * i)) & 0xFF;
        // The char is read in backwards to the register so we need to reverse them
        // So a mask of 512 looks like 0b00000000 00000000 00000001 1111111
        // and not 0b00000000 00000000 10000000 1111111

        job_difficulty_mask[5 - i] = _reverse_bits(value);
    }

    ESP_LOGI(TAG, "Setting ASIC difficulty mask to %d", difficulty);

    send((CMD_WRITE_ALL), job_difficulty_mask, 6, ASIC_SERIALTX_DEBUG);
}

// can ramp up and down in 6.25MHz steps
bool Asic::setAsicFrequency(float target_freq) {
    return doFrequencyTransition(std::numeric_limits<uint8_t>::max(), target_freq);
}

// pass std::numeric_limits<uint8_t>::max() to set all asics
bool Asic::setAsicFrequency(uint8_t asic_index, float target_freq) {
    if (asic_index < std::numeric_limits<uint8_t>::max() && asic_index >= m_asic_count) {
        ESP_LOGE(TAG, "Invalid ASIC index %d, max is %d", asic_index, m_asic_count - 1);
        return false;
    }
    return doFrequencyTransition(asic_index, target_freq);
}


uint8_t Asic::sendWork(uint32_t job_id, bm_job *next_bm_job)
{
    BM1368_job job;

    job.job_id = jobToAsicId(job_id);

    job.num_midstates = 0x01;
    memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
    memcpy(&job.nbits, &next_bm_job->target, 4);
    memcpy(&job.ntime, &next_bm_job->ntime, 4);
    memcpy(job.merkle_root, next_bm_job->merkle_root_be, 32);
    memcpy(job.prev_block_hash, next_bm_job->prev_block_hash_be, 32);
    memcpy(&job.version, &next_bm_job->version, 4);

    send((TYPE_JOB | GROUP_SINGLE | CMD_WRITE), (uint8_t*) &job, sizeof(BM1368_job), ASIC_DEBUG_WORK);

    // we return it because different asics calculate it differently
    return job.job_id;
}

bool Asic::receiveWork(asic_result_t *result)
{
    // wait for a response, wait time is pretty arbitrary
    int received = SERIAL_rx((uint8_t*) result, 11, 60000);

    if (received < 0) {
        ESP_LOGI(TAG, "Error in serial RX");
        return false;
    } else if (received == 0) {
        // Didn't find a solution, restart and try again
        return false;
    }

    if (result->preamble[0] != 0xAA || result->preamble[1] != 0x55) {
        ESP_LOGE(TAG, "Serial RX invalid %i", received);
        ESP_LOG_BUFFER_HEX(TAG, (uint8_t*) result, received);
        SERIAL_clear_buffer();
        return false;
    }

    return true;
}


bool Asic::processWork(task_result *result)
{
    asic_result_t asic_result;
    if (!receiveWork(&asic_result)) {
        return false;
    }

    // if this matches we can assume it's not a nonce but a response from a read request
    if (/*(asic_result.midstate_num == 0) &&*/ !(asic_result.nonce & 0x7f) && !(asic_result.version)) {
        result->data = __bswap32(asic_result.nonce);
        result->reg = asic_result.job_id;
        result->is_reg_resp = 1;
        result->asic_nr = asic_result.midstate_num >> 1;
        return true;
    }

    uint8_t job_id = asicToJobId(asic_result.job_id);

    uint32_t rolled_version = (reverseUint16(asic_result.version) << 13); // shift the 16 bit value left 13

    int asic_nr = nonceToAsicNr(asic_result.nonce);

    result->job_id = job_id;
    result->asic_nr = asic_nr;
    result->nonce = asic_result.nonce;
    result->rolled_version = rolled_version;
    result->is_reg_resp = 0;
    return true;
}

int Asic::getAsicFrequency(uint8_t asic_index) const
{
    if (asic_index < std::numeric_limits<uint8_t>::max() && asic_index >= m_asic_count) {
        ESP_LOGE(TAG, "Invalid ASIC index %d, max is %d", asic_index, m_asic_count - 1);
        return -1;
    }
    if (asic_index == std::numeric_limits<uint8_t>::max()) {
        ESP_LOGI(TAG, "Returning current frequency for all ASICs: %.2f MHz", m_current_frequency);
        return m_current_frequency;
    }
    return m_vCurrentFrequencies[asic_index];
}
