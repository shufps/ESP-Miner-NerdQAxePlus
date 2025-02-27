#include <math.h>

#include "nvs_flash.h"
#include "esp_log.h"

#define TPS53647_EN_PIN GPIO_NUM_10
#define BM1368_RST_PIN GPIO_NUM_1
#define LDO_EN_PIN GPIO_NUM_13

#include "serial.h"
#include "board.h"
#include "nerdqaxeplus.h"
#include "nvs_config.h"
#include "../displays/displayDriver.h"

#include "EMC2302.h"
#include "TMP1075.h"
#include "TPS53647.h"



static const char* TAG="nerdqaxe+";

NerdQaxePlus::NerdQaxePlus() : Board() {
    m_deviceModel = "NerdQAxe+";
    m_miningAgent = m_deviceModel;
    m_version = 501;
    m_asicModel = "BM1368";
    m_asicCount = 4;
    m_asicJobIntervalMs = 1200;
    m_asicFrequency = 490.0;
    m_asicVoltage = 1.25; // default voltage
    m_initVoltage = 1.25;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_numPhases = 2;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    // board params that are passed to the web UI
    m_params.maxPin = 70.0;
    m_params.minPin = 30.0;
    m_params.maxVin = 13.0;
    m_params.minVin = 11.0;

    m_params.minAsicShutdownTemp = 40.0f;
    m_params.maxAsicShutdownTemp = 90.0f;

    m_params.minVRShutdownTemp = 40.0f;
    m_params.maxVRShutdownTemp = 90.0f;

    // absolute values
    m_params.absMinAsicVoltage = 1005;
    m_params.absMaxAsicVoltage = 1400;

    m_params.absMinFrequency = 200;
    m_params.absMaxFrequency = 1000;

    // bm1368 values
    int frequencies[] = {400, 425, 450, 475, 490, 500, 525, 550, 575};
    int voltages[] = {1100, 1150, 1200, 1250, 1300, 1350};
    m_params.setFrequencies(frequencies, sizeof(frequencies)/sizeof(int));
    m_params.setAsicVoltages(voltages, sizeof(voltages)/sizeof(int));

    m_params.defaultFrequency = 490;
    m_params.defaultAsicVoltage = 1250;

    m_asicMaxDifficulty = 1024;
    m_asicMinDifficulty = 256;

#ifdef NERDQAXEPLUS
    m_theme = new ThemeNerdqaxeplus();
#endif

    m_asics = new BM1368();


}

bool NerdQaxePlus::initBoard()
{
    SERIAL_init();

    // Init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initializing failed");
        return false;
    }

    EMC2302_init(m_fanInvertPolarity);
    setFanSpeed(m_fanPerc);

    // configure gpios
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TPS53647_EN_PIN, 0);

    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LDO_EN_PIN, 0);

    gpio_pad_select_gpio(BM1368_RST_PIN);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BM1368_RST_PIN, 0);

    return true;
}

void NerdQaxePlus::shutdown() {
    setVoltage(0.0);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    LDO_disable();

    vTaskDelay(500 / portTICK_PERIOD_MS);
}

bool NerdQaxePlus::initAsics()
{
    // disable buck (disables EN pin)
    setVoltage(0.0);

    // disable LDO
    LDO_disable();

    // set reset low
    gpio_set_level(BM1368_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // enable LDOs
    LDO_enable();

    // wait 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // init buck and enable output
    TPS53647_init(m_numPhases, m_imax, m_ifault);

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage(fmaxf(m_initVoltage, m_asicVoltage));

    // wait 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // release reset pin
    gpio_set_level(BM1368_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    SERIAL_set_baud(m_asics->setMaxBaud());
    SERIAL_clear_buffer();

    vTaskDelay(500 / portTICK_PERIOD_MS);

    // set final output voltage
    setVoltage(m_asicVoltage);

    m_isInitialized = true;
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

bool NerdQaxePlus::getPSUFault() {
    uint16_t vid = TPS53647_get_vout_vid();
    uint8_t status_byte = TPS53647_get_status_byte();

    // if we have 0x97 it means the buck was reset and
    // restarted with VBOOT. In this case we assume there
    // is a PSU error
    // in case of the PSUs over current protection (voltage will drop),
    // we will see bit 3 "VIN_UV" in the status byte
    return ((vid == 0x97) || (status_byte & 0x08));
}

bool NerdQaxePlus::selfTest(){
    //Test Core Voltage
    #define CORE_VOLTAGE_TARGET_MIN 1.1 //mV
    #define CORE_VOLTAGE_TARGET_MAX 1.4 //mV

    char logString[300];

    // Initialize the display
    DisplayDriver *temp_display;
    temp_display = new DisplayDriver();
    temp_display->init(this);

    temp_display->logMessage("\nSelfTest initiated, wait...\r\n\n\n\n\n\n"
                             "[Warning] This test only ensures Asic is properly soldered\nHashrate is not checked");

    //Init Asics
    initAsics();
    float power = getPin();
    float Vout = getVout();
    bool powerOK = (power > m_params.minPin) && (power < m_params.maxPin);
    bool VrOK = (Vout > CORE_VOLTAGE_TARGET_MIN) && (Vout < CORE_VOLTAGE_TARGET_MAX);
    bool allAsicsDetected = (m_chipsDetected == m_asicCount); // Verifica que todos los ASICs se han detectado

    //Warning! This test only ensures Asic is properly soldered
    snprintf(logString, sizeof(logString),  "\nTest result:\r\n"
                                            "- Asics detected [%d/%d]\n"
                                            "- Power status: %s (%.2f W)\n"
                                            "- Asic voltage: %s (%.2f V)\r\n\n"
                                            "%s", // Final result
                                            m_chipsDetected, m_asicCount,
                                            powerOK ? "OK" : "Warning", power,
                                            VrOK ? "OK" : "Warning", Vout,
                                            (allAsicsDetected) ? "OOOOOOOO TEST OK!!! OOOOOOO" : "XXXXXXXXX TEST KO XXXXXXXXX");
    temp_display->logMessage(logString);

    //Update SelfTest flag
    if(allAsicsDetected) nvs_config_set_u16(NVS_CONFIG_SELF_TEST, 0);

    return true;
}
