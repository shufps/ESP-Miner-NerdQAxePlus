#include "bm1368.h"

#include <endian.h>
#include "crc.h"
#include "serial.h"
#include "utils.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "boards/nerdqaxeplus.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif

#define TYPE_JOB 0x20
#define TYPE_CMD 0x40

#define GROUP_SINGLE 0x00
#define GROUP_ALL 0x10

#define CMD_JOB 0x01

#define CMD_SETADDRESS 0x00
#define CMD_WRITE 0x01
#define CMD_READ 0x02
#define CMD_INACTIVE 0x03

#define CMD_WRITE_SINGLE (TYPE_CMD | GROUP_SINGLE | CMD_WRITE)
#define CMD_WRITE_ALL    (TYPE_CMD | GROUP_ALL | CMD_WRITE)
#define CMD_READ_ALL    (TYPE_CMD | GROUP_ALL | CMD_READ)


#define RESPONSE_CMD 0x00
#define RESPONSE_JOB 0x80

#define SLEEP_TIME 20
#define FREQ_MULT 25.0

#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define TICKET_MASK 0x14
#define MISC_CONTROL 0x18

typedef struct __attribute__((__packed__))
{
    uint8_t preamble[2];
    uint32_t nonce;
    uint8_t midstate_num;
    uint8_t job_id;
    uint16_t version;
    uint8_t crc;
} asic_result_t;

static const char *TAG = "bm1368Module";

static const uint8_t chip_id[6] = {0xaa, 0x55, 0x13, 0x68, 0x00, 0x00};

static float current_frequency = 56.25;

/// @brief
/// @param ftdi
/// @param header
/// @param data
/// @param len
static void _send_BM1368(uint8_t header, uint8_t *data, uint8_t data_len, bool debug)
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

static void _send6(uint8_t header, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5) {
    uint8_t buf[6] = {b0, b1, b2, b3, b4, b5};
    _send_BM1368(header, buf, sizeof(buf), BM1368_SERIALTX_DEBUG);
}

static void _send2(uint8_t header, uint8_t b0, uint8_t b1) {
    uint8_t buf[6] = {b0, b1};
    _send_BM1368(header, buf, sizeof(buf), BM1368_SERIALTX_DEBUG);
}

static void _send_chain_inactive(void)
{
    _send2(TYPE_CMD | GROUP_ALL | CMD_INACTIVE, 0x00, 0x00);
}

static void _set_chip_address(uint8_t chipAddr)
{
    _send2(TYPE_CMD | GROUP_SINGLE | CMD_SETADDRESS, chipAddr, 0x00);
}


// Function to set the hash frequency
// gives the same PLL settings as the S21 dumps
bool send_hash_frequency(float target_freq) {
    float max_diff = 0.001;
    uint8_t freqbuf[6] = {0x00, 0x08, 0x40, 0xA0, 0x02, 0x41};
    int postdiv_min = 255;
    int postdiv2_min = 255;
    int refdiv, fb_divider, postdiv1, postdiv2;
    float newf;
    bool found = false;
    int best_refdiv, best_fb_divider, best_postdiv1, best_postdiv2;
    float best_newf;

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
        return false;
    }

    freqbuf[2] = (best_fb_divider * 25 / best_refdiv >= 2400) ? 0x50 : 0x40;
    freqbuf[3] = best_fb_divider;
    freqbuf[4] = best_refdiv;
    freqbuf[5] = (((best_postdiv1 - 1) & 0xf) << 4) | ((best_postdiv2 - 1) & 0xf);

    _send_BM1368(CMD_WRITE_ALL, freqbuf, sizeof(freqbuf), BM1368_SERIALTX_DEBUG);
    //ESP_LOG_BUFFER_HEX(TAG, freqbuf, sizeof(freqbuf));

    ESP_LOGI(TAG, "Setting Frequency to %.2fMHz (%.2f)", target_freq, best_newf);
    current_frequency = target_freq;
    return true;
}

// Function to perform frequency transition up or down
bool do_frequency_transition(float target_frequency) {
    float step = 6.25;
    float current = current_frequency;
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
        if (!send_hash_frequency(current)) {
            printf("ERROR: Failed to set frequency to %.2f MHz\n", current);
            return false;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Ramp in the appropriate direction
    while ((direction > 0 && current < target) || (direction < 0 && current > target)) {
        float next_step = fmin(fabs(direction), fabs(target - current));
        current += direction > 0 ? next_step : -next_step;
        if (!send_hash_frequency(current)) {
            printf("ERROR: Failed to set frequency to %.2f MHz\n", current);
            return false;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Set the exact target frequency to finalize
    if (!send_hash_frequency(target)) {
        printf("ERROR: Failed to set frequency to %.2f MHz\n", target);
        return false;
    }
    return true;
}

// can ramp up and down in 6.25MHz steps
bool BM1368_send_hash_frequency(float target_freq) {
    return do_frequency_transition(target_freq);
}

static int _count_asics() {

    // read register 00 on all chips (should respond AA 55 13 68 00 00 00 00 00 00 0F)
    _send2(CMD_READ_ALL, 0x00, 0x00);

    uint8_t buf[11];
    int chip_counter = 0;
    while (SERIAL_rx(buf, sizeof(buf), 1000) > 0) {
        if (!strncmp((char *) chip_id, (char *) buf, sizeof(chip_id))) {
            chip_counter++;
            ESP_LOGI(TAG, "found asic #%d", chip_counter);
        } else {
            ESP_LOGE(TAG, "unexpected response ... ignoring ...");
            ESP_LOG_BUFFER_HEX(TAG, buf, sizeof(buf));
        }
    }
    return chip_counter;
}



static uint8_t _send_init(uint64_t frequency, uint16_t asic_count)
{

    // enable and set version rolling mask to 0xFFFF
    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    int chip_counter = _count_asics();
    ESP_LOGI(TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    // enable and set version rolling mask to 0xFFFF (again)
    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // Reg_A8
    _send6(CMD_WRITE_ALL, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00);

    // Misc Control
    _send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);

    // chain inactive
    _send_chain_inactive();

    // set chip address
    for (uint8_t i = 0; i < chip_counter; i++) {
        _set_chip_address(i * 2);
    }

    // Core Register Control
    _send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8B, 0x00);

    // Core Register Control
    _send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x18);

    BM1368_set_job_difficulty_mask(BM1368_INITIAL_DIFFICULTY);

    // Analog Mux Control
    _send6(CMD_WRITE_ALL, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03);

    // Set the IO Driver Strength on chip 00
    _send6(CMD_WRITE_ALL, 0x00, 0x58, 0x02, 0x11, 0x11, 0x11);

    for (uint8_t i = 0; i < chip_counter; i++) {
        // Reg_A8
        _send6(CMD_WRITE_SINGLE, i * 2, 0xA8, 0x00, 0x07, 0x01, 0xF0);
        // Misc Control
        _send6(CMD_WRITE_SINGLE, i * 2, 0x18, 0xF0, 0x00, 0xC1, 0x00);
        // Core Register Control
        _send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x8B, 0x00);
        // Core Register Control
        _send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x80, 0x18);
        // Core Register Control
        _send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    do_frequency_transition(frequency);

    // register 10 is still a bit of a mystery. discussion: https://github.com/skot/ESP-Miner/pull/167

    // _send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x11, 0x5A); //S19k Pro Default
    // _send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x14, 0x46); //S19XP-Luxos Default
    // _send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0x1C); //S19XP-Stock Default
    // _send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x0F, 0x00, 0x00); //supposedly the "full" 32bit nonce range
    _send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0xA4); // S21-Stock Default

    _send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}

// reset the BM1368 via the RTS line
static void _reset(void)
{
    gpio_set_level(BM1368_RST_PIN, 0);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // set the gpio pin high
    gpio_set_level(BM1368_RST_PIN, 1);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

static void _send_read_address(void)
{
    _send2(TYPE_CMD | GROUP_ALL | CMD_READ, 0x00, 0x00);
}

uint8_t BM1368_init(uint64_t frequency, uint16_t asic_count)
{
    ESP_LOGI(TAG, "Initializing BM1368");

    // esp_rom_gpio_pad_select_gpio(BM1368_RST_PIN);
    gpio_pad_select_gpio(BM1368_RST_PIN);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);

    // reset the bm1368
    _reset();

    // empty serial buffer because it could contain nonces from before the reset
    // if there was no power cycle and asic chips counting would fail
    SERIAL_clear_buffer();

    return _send_init(frequency, asic_count);
}

int BM1368_set_max_baud(void)
{
    return 115749;
/*
    /// return 115749;

    // divider of 0 for 3,125,000
    ESP_LOGI(TAG, "Setting max baud of 1000000 ");

    unsigned char init8[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    _send_simple(init8, 11);
    return 1000000;
*/
}

void BM1368_set_job_difficulty_mask(int difficulty)
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

    _send_BM1368((CMD_WRITE_ALL), job_difficulty_mask, 6, BM1368_SERIALTX_DEBUG);
}

uint8_t BM1368_send_work(uint32_t job_id, bm_job *next_bm_job)
{
    BM1368_job job;

    // job-IDs: 00, 18, 30, 48, 60, 78, 10, 28, 40, 58, 70, 08, 20, 38, 50, 68
    job.job_id = (job_id * 24) & 0x7f;

    job.num_midstates = 0x01;
    memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
    memcpy(&job.nbits, &next_bm_job->target, 4);
    memcpy(&job.ntime, &next_bm_job->ntime, 4);
    memcpy(job.merkle_root, next_bm_job->merkle_root_be, 32);
    memcpy(job.prev_block_hash, next_bm_job->prev_block_hash_be, 32);
    memcpy(&job.version, &next_bm_job->version, 4);

    _send_BM1368((TYPE_JOB | GROUP_SINGLE | CMD_WRITE), (uint8_t*) &job, sizeof(BM1368_job), BM1368_DEBUG_WORK);

    // we return it because different asics calculate it differently
    return job.job_id;
}

static bool _receive_work(asic_result_t *result)
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

static uint16_t reverse_uint16(uint16_t num)
{
    return (num >> 8) | (num << 8);
}

bool BM1368_proccess_work(task_result *result)
{
    asic_result_t asic_result;
    if (!_receive_work(&asic_result)) {
        return false;
    }

    // if this matches we can assume it's not a nonce but a response from a read request
    if ((asic_result.midstate_num == 0) && !(asic_result.nonce & 0x7f) && !(asic_result.version)) {
        result->data = __bswap32(asic_result.nonce);
        result->reg = asic_result.job_id;
        result->is_reg_resp = 1;
        return true;
    }

    uint8_t job_id = (asic_result.job_id & 0xf0) >> 1;

    uint32_t rolled_version = (reverse_uint16(asic_result.version) << 13); // shift the 16 bit value left 13

    int asic_nr = (asic_result.nonce & 0x0000fc00) >> 10;

    result->job_id = job_id;
    result->asic_nr = asic_nr;
    result->nonce = asic_result.nonce;
    result->rolled_version = rolled_version;
    result->is_reg_resp = 0;
    return true;
}
