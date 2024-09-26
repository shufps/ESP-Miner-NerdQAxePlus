#include "nvs_flash.h"
#include "esp_log.h"

#define TPS53647_EN_PIN GPIO_NUM_10
#define BM1368_RST_PIN GPIO_NUM_1
#define LDO_EN_PIN GPIO_NUM_13

#include "serial.h"
#include "board.h"
#include "nerdqaxeplus.h"
#include "nvs_config.h"

#include "EMC2302.h"
#include "TMP1075.h"
#include "TPS53647.h"



static const char* TAG="nerdqaxe+";

NerdQaxePlus::NerdQaxePlus() : Board() {
    device_model = "NerdQAxe+";
    version = 501;
    asic_model = "BM1368";
    asic_count = 4;
    asic_job_frequency_ms = 1500;
    asic_frequency = 490.0;
    asic_voltage = 1.20;
    fan_invert_polarity = false;
    fan_perc = 100;
    num_tps_phases = 2;

    asic_max_difficulty = 1024;
    asic_min_difficulty = 256;

    theme = new Theme();
    theme->ui_img_btcscreen = &ui_img_nerdqaxeplus_btcscreen_png;
    theme->ui_img_initscreen = &ui_img_nerdqaxeplus_initscreen2_png;
    theme->ui_img_miningscreen = &ui_img_nerdqaxeplus_miningscreen2_png;
    theme->ui_img_portalscreen = &ui_img_nerdqaxeplus_portalscreen_png;
    theme->ui_img_settingscreen = &ui_img_nerdqaxeplus_settingsscreen_png;
    theme->ui_img_splashscreen = &ui_img_nerdqaxeplus_splashscreen2_png;
}

bool NerdQaxePlus::init()
{
    SERIAL_init();

    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    EMC2302_init(fan_invert_polarity);
    set_fan_speed(fan_perc);

    // configure gpios
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_pad_select_gpio(BM1368_RST_PIN);

    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);

    // disable buck (disables EN pin)
    set_voltage(0.0);

    // disable LDO
    LDO_disable();

    // set reset high
    gpio_set_level(BM1368_RST_PIN, 1);

    // wait 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // enable LDOs
    LDO_enable();

    // wait 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // init buck and enable output
    TPS53647_init(num_tps_phases);
    set_voltage(asic_voltage / 1000.0);

    // wait 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // release reset pin
    gpio_set_level(BM1368_RST_PIN, 1);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    SERIAL_clear_buffer();
    if (!asics.init(asic_frequency, asic_count, asic_max_difficulty)) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    SERIAL_set_baud(asics.set_max_baud());
    SERIAL_clear_buffer();

    vTaskDelay(500 / portTICK_PERIOD_MS);
    return true;
}

void NerdQaxePlus::LDO_enable()
{
    ESP_LOGI(TAG, "Enabled LDOs");
    gpio_set_level(LDO_EN_PIN, 1);
}

void NerdQaxePlus::LDO_disable()
{
    ESP_LOGI(TAG, "Disable LDOs");
    gpio_set_level(LDO_EN_PIN, 0);
}

bool NerdQaxePlus::set_voltage(float core_voltage)
{
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    TPS53647_set_vout(core_voltage);
    return true;
}

uint16_t NerdQaxePlus::get_voltage_mv()
{
    return TPS53647_get_vout() * 1000.0f;
}

void NerdQaxePlus::set_fan_speed(float perc) {
    EMC2302_set_fan_speed(perc);
}

void NerdQaxePlus::get_fan_speed(uint16_t* rpm) {
    EMC2302_get_fan_speed(rpm);
}

float NerdQaxePlus::read_temperature(int index) {
    return TMP1075_read_temperature(index);
}

float NerdQaxePlus::get_vin() {
    return TPS53647_get_vin();
}

float NerdQaxePlus::get_iin() {
    return TPS53647_get_iin();
}

float NerdQaxePlus::get_pin() {
    return TPS53647_get_pin();
}

float NerdQaxePlus::get_vout() {
    return TPS53647_get_vout();
}

float NerdQaxePlus::get_iout() {
    return TPS53647_get_iout();
}

float NerdQaxePlus::get_pout() {
    return TPS53647_get_pout();
}

