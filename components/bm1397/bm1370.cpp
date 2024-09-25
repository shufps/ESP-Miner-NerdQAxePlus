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
#include "bm1370.h"

#include "crc.h"
#include "serial.h"
#include "utils.h"


static const char *TAG = "bm1370Module";

static const uint8_t chip_id[6] = {0xaa, 0x55, 0x13, 0x70, 0x00, 0x00};

BM1370::BM1370() : BM1368() {
    // NOP
}

const uint8_t* BM1370::get_chip_id() {
    return (uint8_t*) chip_id;
}

uint8_t BM1370::init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty)
{
    // reset is done externally to not have board dependencies

    // enable and set version rolling mask to 0xFFFF
    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    int chip_counter = this->count_asics();
    ESP_LOGI(TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    // enable and set version rolling mask to 0xFFFF (again)
    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // Reg_A8
    this->send6(CMD_WRITE_ALL, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00);

    // Misc Control
    //this->send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);
    this->send6(CMD_WRITE_ALL, 0x00, 0x18, 0xF0, 0x00, 0xC1, 0x00);

    // chain inactive
    this->send_chain_inactive();

    // set chip address
    for (uint8_t i = 0; i < chip_counter; i++) {
        this->set_chip_address(i * 2);
    }

    // Core Register Control
    this->send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8B, 0x00);

    // Core Register Control
    //this->send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x18);
    this->send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x0C);

    this->set_job_difficulty_mask(difficulty);

    // Set the IO Driver Strength on chip 00
    this->send6(CMD_WRITE_ALL, 0x00, 0x58, 0x00, 0x01, 0x11, 0x11);

    // ?
    this->send6(CMD_WRITE_ALL, 0x00, 0x68, 0x5A, 0xA5, 0x5A, 0xA5);

    // ?
    this->send6(CMD_WRITE_ALL, 0x00, 0x28, 0x01, 0x30, 0x00, 0x00);

    for (uint8_t i = 0; i < chip_counter; i++) {
        // Reg_A8
        this->send6(CMD_WRITE_SINGLE, i * 2, 0xA8, 0x00, 0x07, 0x01, 0xF0);
        // Misc Control
        this->send6(CMD_WRITE_SINGLE, i * 2, 0x18, 0xF0, 0x00, 0xC1, 0x00);
        // Core Register Control
        this->send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x8B, 0x00);
        // Core Register Control
        this->send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x80, 0x0C);
        // Core Register Control
        this->send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    // ?
    this->send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Analog Mux Control
    this->send6(CMD_WRITE_ALL, 0x00, 0x54, 0x00, 0x00, 0x00, 0x02);

    // ?
    this->send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Core Register Control
    this->send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8D, 0xEE);

    do_frequency_transition(frequency);

    // register 10 is still a bit of a mystery. discussion: https://github.com/skot/ESP-Miner/pull/167

    // this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x11, 0x5A); //S19k Pro Default
    // this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x14, 0x46); //S19XP-Luxos Default
    // this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0x1C); //S19XP-Stock Default
    // this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x0F, 0x00, 0x00); //supposedly the "full" 32bit nonce range
    // this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x15, 0xA4); // S21-Stock Default
    this->send6(CMD_WRITE_ALL, 0x00, 0x10, 0x00, 0x00, 0x1e, 0xB5); // S21-Pro

    this->send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}
