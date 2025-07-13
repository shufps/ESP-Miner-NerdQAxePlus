#pragma once

#include "driver/i2c.h"

#define I2C_MASTER_NUM ((i2c_port_t) 0)
#define I2C_MASTER_TIMEOUT_MS pdMS_TO_TICKS(1000)

esp_err_t i2c_master_init(void);
esp_err_t i2c_master_delete(void);
esp_err_t i2c_master_register_read(uint8_t device_address, uint8_t reg_addr, uint8_t *data, size_t len);
esp_err_t i2c_master_register_write_byte(uint8_t device_address, uint8_t reg_addr, uint8_t data);
esp_err_t i2c_master_register_write_word(uint8_t device_address, uint8_t reg_addr, uint16_t data);

