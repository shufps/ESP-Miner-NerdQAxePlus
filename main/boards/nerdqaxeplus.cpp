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
    m_deviceModel = "NerdQAxe+";
    m_version = 501;
    m_asicModel = "BM1368";
    m_asicCount = 4;
    m_asicJobIntervalMs = 1500;
    m_asicFrequency = 490.0;
    m_asicVoltage = 1.20;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_numPhases = 2;

    m_asicMaxDifficulty = 1024;
    m_asicMinDifficulty = 256;

    m_theme = new ThemeNerdqaxeplus();
}

bool NerdQaxePlus::init()
{
    SERIAL_init();

    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    EMC2302_init(m_fanInvertPolarity);
    setFanSpeed(m_fanPerc);

    // configure gpios
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_pad_select_gpio(BM1368_RST_PIN);

    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);

    // disable buck (disables EN pin)
    setVoltage(0.0);

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
    TPS53647_init(m_numPhases);
    setVoltage(m_asicVoltage / 1000.0);

    // wait 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // release reset pin
    gpio_set_level(BM1368_RST_PIN, 1);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    SERIAL_clear_buffer();
    if (!asics.init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty)) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    SERIAL_set_baud(asics.setMaxBaud());
    SERIAL_clear_buffer();

    vTaskDelay(500 / portTICK_PERIOD_MS);
    return true;
}

void NerdQaxePlus::requestBuckTelemtry() {
    TPS53647_status();
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

bool NerdQaxePlus::setVoltage(float core_voltage)
{
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    TPS53647_set_vout(core_voltage);
    return true;
}

uint16_t NerdQaxePlus::getVoltageMv()
{
    return TPS53647_get_vout() * 1000.0f;
}

void NerdQaxePlus::setFanSpeed(float perc) {
    EMC2302_set_fan_speed(perc);
}

void NerdQaxePlus::getFanSpeed(uint16_t* rpm) {
    EMC2302_get_fan_speed(rpm);
}

float NerdQaxePlus::readTemperature(int index) {
    return TMP1075_read_temperature(index);
}

float NerdQaxePlus::getVin() {
    return TPS53647_get_vin();
}

float NerdQaxePlus::getIin() {
    return TPS53647_get_iin();
}

float NerdQaxePlus::getPin() {
    return TPS53647_get_pin();
}

float NerdQaxePlus::getVout() {
    return TPS53647_get_vout();
}

float NerdQaxePlus::getIout() {
    return TPS53647_get_iout();
}

float NerdQaxePlus::getPout() {
    return TPS53647_get_pout();
}

