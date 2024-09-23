#include "board.h"
#include "nvs_flash.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "serial.h"
#include "EMC2302.h"
#include "TMP1075.h"
#include "TPS53647.h"

static const char* TAG="nerdqaxe+";

volatile board_t nerdqaxeplus = {
    .device_model = "NerdQAxe+",
    .version = 501,
    .asic_model = "BM1368",
    .asic_count = 4,
    .asic_job_frequency_ms = 1500,
    .asic_voltage = 1.20,
    .asic_frequency = 490.0,
    .asic_initial_difficulty = BM1368_INITIAL_DIFFICULTY
};

static board_t *self = &nerdqaxeplus;

const char *board_get_device_model()
{
    return self->device_model;
}

const char *board_get_asic_model()
{
    return self->asic_model;
}

int board_get_asic_count()
{
    return self->asic_count;
}

double board_get_asic_job_frequency_ms()
{
    return self->asic_job_frequency_ms;
}

float board_get_asic_frequency() {
    return self->asic_frequency;
}

float board_get_asic_voltage() {
    return self->asic_voltage;
}

uint32_t board_get_initial_ASIC_difficulty()
{
    return self->asic_initial_difficulty;
}

int board_get_version() {
    return self->version;
}

bool board_asic_proccess_work(task_result *result) {
    return BM1368_proccess_work(result);
}

int board_asic_set_max_baud(void) {
    return BM1368_set_max_baud();
}

void board_asic_set_job_difficulty_mask(uint32_t mask) {
    BM1368_set_job_difficulty_mask(mask);
}

uint8_t board_asic_send_work(uint32_t job_id, bm_job *next_bm_job) {
    return BM1368_send_work(job_id, next_bm_job);
}

bool board_asic_send_hash_frequency(float frequency) {
    return BM1368_send_hash_frequency(frequency);
}

void board_load_settings() {
    self->asic_frequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    self->asic_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    self->fan_invert_polarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1);
    self->fan_perc = nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %.3f", (float) self->asic_frequency);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_VOLTAGE %.3f", (float) self->asic_voltage / 1000.0f);
    ESP_LOGI(TAG, "NVS_CONFIG_INVERT_FAN_POLARITY %s", self->fan_invert_polarity ? "true" : "false");
    ESP_LOGI(TAG, "NVS_CONFIG_FAN_SPEED %d%%", (int) self->fan_perc);
}

bool board_init()
{
    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    EMC2302_init(self->fan_invert_polarity);
    board_set_fan_speed(self->fan_perc);

    // configure gpios
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_pad_select_gpio(BM1368_RST_PIN);

    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);

    // disable buck (disabled EN pin)
    board_set_voltage(0.0);

    // disable LDO
    board_LDO_disable();

    // set reset high
    gpio_set_level(BM1368_RST_PIN, 1);

    // wait 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // enable LDOs
    board_LDO_enable();

    // wait 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // init buck and enable output
    TPS53647_init();
    board_set_voltage(self->asic_voltage / 1000.0);

    // wait 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);

    SERIAL_init();
    if (!BM1368_init(self->asic_frequency, self->asic_count)) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    SERIAL_set_baud(board_asic_set_max_baud());
    SERIAL_clear_buffer();

    vTaskDelay(500 / portTICK_PERIOD_MS);
    return true;
}

void board_LDO_enable()
{
    ESP_LOGI(TAG, "Enabled LDOs");
    gpio_set_level(LDO_EN_PIN, 1);
}

void board_LDO_disable()
{
    ESP_LOGI(TAG, "Disable LDOs");
    gpio_set_level(LDO_EN_PIN, 0);
}

bool board_set_voltage(float core_voltage)
{
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    TPS53647_set_vout(core_voltage);
    return true;
}

uint16_t board_get_voltage_mv()
{
    return TPS53647_get_vout() * 1000.0f;
}

void board_set_fan_speed(float perc) {
    EMC2302_set_fan_speed(perc);
}

void board_get_fan_speed(uint16_t* rpm) {
    EMC2302_get_fan_speed(rpm);
}

float board_read_temperature(int index) {
    return TMP1075_read_temperature(index);
}

float board_get_vin() {
    return TPS53647_get_vin();
}

float board_get_iin() {
    return TPS53647_get_iin();
}

float board_get_pin() {
    return TPS53647_get_pin();
}

float board_get_vout() {
    return TPS53647_get_vout();
}

float board_get_iout() {
    return TPS53647_get_iout();
}

float board_get_pout() {
    return TPS53647_get_pout();
}

