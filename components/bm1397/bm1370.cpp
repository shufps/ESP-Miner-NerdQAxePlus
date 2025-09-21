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

static const uint64_t BM1370_CORE_COUNT = 128;
static const uint64_t BM1370_SMALL_CORE_COUNT = 2040;

#define REG_NONCE_TOTAL_CNT 0x8c

BM1370::BM1370() : Asic() {
    // NOP
}

const uint8_t* BM1370::getChipId() {
    return (uint8_t*) chip_id;
}

uint32_t BM1370::getDefaultVrFrequency() {
    return vrRegToFreq(0x1eb5);
};

uint8_t BM1370::init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency)
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
    ESP_LOGIE(chip_counter == asic_count, TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // Reg_A8
    send6(CMD_WRITE_ALL, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00);

    // Misc Control
    //send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);
    send6(CMD_WRITE_ALL, 0x00, 0x18, 0xF0, 0x00, 0xC1, 0x00);

    // chain inactive
    sendChainInactive();

    // set chip address
    for (uint8_t i = 0; i < chip_counter; i++) {
        setChipAddress(i * 4);
    }

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8B, 0x00);

    // Core Register Control
    //send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x18);
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x0C);

    setJobDifficultyMask(difficulty);

    // Set the IO Driver Strength on chip 00
    send6(CMD_WRITE_ALL, 0x00, 0x58, 0x00, 0x01, 0x11, 0x11);

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0x68, 0x5A, 0xA5, 0x5A, 0xA5);

    // set baud
    //send6(CMD_WRITE_ALL, 0x00, 0x28, 0x01, 0x30, 0x00, 0x00);

    for (uint8_t i = 0; i < chip_counter; i++) {
        // Reg_A8
        send6(CMD_WRITE_SINGLE, i * 4, 0xA8, 0x00, 0x07, 0x01, 0xF0);
        // Misc Control
        send6(CMD_WRITE_SINGLE, i * 4, 0x18, 0xF0, 0x00, 0xC1, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 4, 0x3C, 0x80, 0x00, 0x8B, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 4, 0x3C, 0x80, 0x00, 0x80, 0x0C);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 4, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Analog Mux Control
    send6(CMD_WRITE_ALL, 0x00, 0x54, 0x00, 0x00, 0x00, 0x02);

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8D, 0xEE);

    doFrequencyTransition(frequency);

    // set 0x10
    setVrFrequency(vrFrequency);

    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}

uint8_t BM1370::jobToAsicId(uint8_t job_id) {
    // job-IDs: 00, 18, 30, 48, 60, 78, 10, 28, 40, 58, 70, 08, 20, 38, 50, 68
    return (job_id * 24) & 0x7f;
}

uint8_t BM1370::asicToJobId(uint8_t asic_id) {
    return (asic_id & 0xf0) >> 1;
}

uint8_t BM1370::nonceToAsicNr(uint32_t nonce) {
    return (uint8_t) ((nonce & 0x0000fc00) >> 11);
}

uint8_t BM1370::chipIndexFromAddr(uint8_t addr) {
    return addr >> 2;
}

uint8_t BM1370::addrFromChipIndex(uint8_t idx) {
    return idx << 2;
}

void BM1370::resetCounter(uint8_t reg) {
    send6(CMD_WRITE_ALL, 0x00, reg, 0x00, 0x00, 0x00, 0x00);
}

void BM1370::readCounter(uint8_t reg) {
    send2(CMD_READ_ALL, 0x00, reg);
}

uint16_t BM1370::getSmallCoreCount() {
    return BM1370_SMALL_CORE_COUNT;
}

