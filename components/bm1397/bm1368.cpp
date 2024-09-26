#include <endian.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "bm1368.h"

#include "crc.h"
#include "serial.h"
#include "utils.h"


static const char *TAG = "bm1368Module";

static const uint8_t chip_id[6] = {0xaa, 0x55, 0x13, 0x68, 0x00, 0x00};

BM1368::BM1368() : Asic() {
    // NOP
}

const uint8_t* BM1368::get_chip_id() {
    return (uint8_t*) chip_id;
}

uint8_t BM1368::init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty)
{
    // reset is done externally to not have board dependencies

    // enable and set version rolling mask to 0xFFFF
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    int chip_counter = count_asics();
    ESP_LOGI(TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // Reg_A8
    send6(CMD_WRITE_ALL, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00);

    // Misc Control
    send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);

    // chain inactive
    send_chain_inactive();

    // set chip address
    for (uint8_t i = 0; i < chip_counter; i++) {
        set_chip_address(i * 2);
    }

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8B, 0x00);

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x18);

    set_job_difficulty_mask(difficulty);

    // Analog Mux Control
    send6(CMD_WRITE_ALL, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03);

    // Set the IO Driver Strength on chip 00
    send6(CMD_WRITE_ALL, 0x00, 0x58, 0x02, 0x11, 0x11, 0x11);

    for (uint8_t i = 0; i < chip_counter; i++) {
        // Reg_A8
        send6(CMD_WRITE_SINGLE, i * 2, 0xA8, 0x00, 0x07, 0x01, 0xF0);
        // Misc Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x18, 0xF0, 0x00, 0xC1, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x8B, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x80, 0x18);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    do_frequency_transition(frequency);

    // register 10 is still a bit of a mystery. discussion: https://github.com/skot/ESP-Miner/pull/167

    // send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x11, 0x5A); //S19k Pro Default
    // send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x14, 0x46); //S19XP-Luxos Default
    // send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0x1C); //S19XP-Stock Default
    // send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x0F, 0x00, 0x00); //supposedly the "full" 32bit nonce range
    send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0xA4); // S21-Stock Default

    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}

int BM1368::set_max_baud(void)
{
    return 115749;
/*

    // divider of 0 for 3,125,000
    ESP_LOGI(TAG, "Setting max baud of 1000000 ");

    unsigned char init8[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    _send_simple(init8, 11);
    return 1000000;
*/
}

void BM1368::requestChipTemp() {
    send2(CMD_READ_ALL, 0x00, 0xB4);
    send6(CMD_WRITE_ALL, 0x00, 0xB0, 0x80, 0x00, 0x00, 0x00);
    send6(CMD_WRITE_ALL, 0x00, 0xB0, 0x00, 0x02, 0x00, 0x00);
    send6(CMD_WRITE_ALL, 0x00, 0xB0, 0x01, 0x02, 0x00, 0x00);
    send6(CMD_WRITE_ALL, 0x00, 0xB0, 0x10, 0x02, 0x00, 0x00);
}

uint8_t BM1368::job_to_asic_id(uint8_t job_id) {
    // job-IDs: 00, 18, 30, 48, 60, 78, 10, 28, 40, 58, 70, 08, 20, 38, 50, 68
    return (job_id * 24) & 0x7f;
}

uint8_t BM1368::asic_to_job_id(uint8_t asic_id) {
    return (asic_id & 0xf0) >> 1;
}

