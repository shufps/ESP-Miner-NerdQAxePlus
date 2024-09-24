#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"

const static char* TAG = "board";

Board::Board() {
    // NOP
}

void Board::load_settings()
{
    this->asic_frequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    this->asic_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    this->fan_invert_polarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1);
    this->fan_perc = nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %.3f", (float) this->asic_frequency);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_VOLTAGE %.3f", (float) this->asic_voltage / 1000.0f);
    ESP_LOGI(TAG, "NVS_CONFIG_INVERT_FAN_POLARITY %s", this->fan_invert_polarity ? "true" : "false");
    ESP_LOGI(TAG, "NVS_CONFIG_FAN_SPEED %d%%", (int) this->fan_perc);
}

const char *Board::get_device_model()
{
    return this->device_model;
}

int Board::get_version()
{
    return this->version;
}

const char *Board::get_asic_model()
{
    return this->asic_model;
}

int Board::get_asic_count()
{
    return this->asic_count;
}

double Board::get_asic_job_frequency_ms()
{
    return this->asic_job_frequency_ms;
}

uint32_t Board::get_initial_ASIC_difficulty()
{
    return this->asic_initial_difficulty;
}

bool Board::asic_proccess_work(task_result *result) {
    return this->get_asics()->proccess_work(result);
}

int Board::asic_set_max_baud(void) {
    return this->get_asics()->set_max_baud();
}

void Board::asic_set_job_difficulty_mask(uint32_t mask) {
    this->get_asics()->set_job_difficulty_mask(mask);
}

uint8_t Board::asic_send_work(uint32_t job_id, bm_job *next_bm_job) {
    return this->get_asics()->send_work(job_id, next_bm_job);
}

bool Board::asic_send_hash_frequency(float frequency) {
    return this->get_asics()->set_hash_frequency(frequency);
}