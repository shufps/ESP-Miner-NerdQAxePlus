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
#include "bm1366.h"

#include "crc.h"
#include "serial.h"
#include "utils.h"

static const uint64_t BM1366_CORE_COUNT = 112;
static const uint64_t BM1366_SMALL_CORE_COUNT = 894;

static const char *TAG = "bm1366Module";

static const uint8_t chip_id[6] = {0xaa, 0x55, 0x13, 0x66, 0x00, 0x00};

BM1366::BM1366() : Asic() {
    // NOP
}

const uint8_t* BM1366::getChipId() {
    return (uint8_t*) chip_id;
}

uint32_t BM1366::getDefaultVrFrequency() {
    return vrRegToFreq(0x151c);
};

uint8_t BM1366::init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency)
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
    send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);

    // chain inactive
    sendChainInactive();

    // set chip address
    for (uint8_t i = 0; i < chip_counter; i++) {
        setChipAddress(i * 2);
    }

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x85, 0x40);

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x20);

    setJobDifficultyMask(difficulty);

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
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x85, 0x40);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x80, 0x20);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, i * 2, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    doFrequencyTransition(frequency);

    // set 0x10
    setVrFrequency(vrFrequency);

    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}

int BM1366::setMaxBaud(void)
{
//    return 115749;
    ESP_LOGI(TAG, "Setting max baud of 1000000 ");
    send6(CMD_WRITE_ALL, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00);
    return 1000000;
}

uint8_t BM1366::jobToAsicId(uint8_t job_id) {
    return (job_id * 8) & 0x7f;
}

uint8_t BM1366::asicToJobId(uint8_t asic_id) {
    return asic_id & 0xf8;
}

uint8_t BM1366::nonceToAsicNr(uint32_t nonce) {
    return (uint8_t) ((nonce & 0x0000fc00) >> 10);
}

uint16_t BM1366::getSmallCoreCount() {
    return BM1366_SMALL_CORE_COUNT;
}
