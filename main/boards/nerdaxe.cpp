#include <math.h>

#include "nvs_flash.h"
#include "esp_log.h"

#define PWR_EN_PIN GPIO_NUM_10
#define BM1366_RST_PIN GPIO_NUM_1

#include "serial.h"
#include "board.h"
#include "nerdaxe.h"
#include "nvs_config.h"
#include "../displays/displayDriver.h"

#include "drivers/nerdaxe/DS4432U.h"
#include "drivers/nerdaxe/EMC2101.h"
#include "drivers/nerdaxe/INA260.h"
#include "drivers/nerdaxe/adc.h"

static const char* TAG="nerdaxe";

#define TPS40305_VFB 0.6

// DS4432U Transfer function constants for Bitaxe board
// #define BITAXE_RFS 80000.0     // R16
// #define BITAXE_IFS ((DS4432_VRFS * 127.0) / (BITAXE_RFS * 16))
#define BITAXE_IFS 0.000098921 // (Vrfs / Rfs) x (127/16)  -> Vrfs = 0.997, Rfs = 80000
#define BITAXE_RA 4750.0       // R14
#define BITAXE_RB 3320.0       // R15
#define BITAXE_VNOM 1.451   // this is with the current DAC set to 0. Should be pretty close to (VFB*(RA+RB))/RB
#define BITAXE_VMAX 2.39
#define BITAXE_VMIN 0.046

NerdAxe::NerdAxe() : Board() {
    m_deviceModel = "NerdAxe";
    m_miningAgent = m_deviceModel;
    m_version = 204;
    m_asicModel = "BM1366";
    m_asicCount = 1;
    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {400, 425, 450, 475, 485, 500, 525, 550, 575};
    m_asicVoltages = {1100, 1150, 1200, 1250, 1300};
    m_defaultAsicFrequency = m_asicFrequency = 485;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1200;
    m_fanInvertPolarity = true;
    m_fanPerc = 100;
    m_flipScreen = true;
    m_numTempSensors = 1;

    m_pidSettings.targetTemp = 55;
    m_pidSettings.p =  400; // 2.00
    m_pidSettings.i =   10; // 0.1
    m_pidSettings.d = 1000; // 10.00

    m_maxPin = 15.0;
    m_minPin = 5.0;
    m_maxVin = 5.5;
    m_minVin = 4.5;

    m_asicMaxDifficulty = 256;
    m_asicMinDifficulty = 64;

#ifdef NERDAXE
    m_theme = new ThemeNerdaxe();
#endif

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1366();
}

/**
 * @brief ds4432_tps40305_bitaxe_voltage_to_reg takes a voltage and returns a register setting for the DS4432U to get that voltage on the TPS40305
 * careful with this one!!
 */
uint8_t NerdAxe::ds4432_tps40305_bitaxe_voltage_to_reg(float vout)
{
    float change;
    uint8_t reg;

    // make sure the requested voltage is in within range of BITAXE_VMIN and BITAXE_VMAX
    if (vout > BITAXE_VMAX || vout < BITAXE_VMIN)
    {
        return 0;
    }

    // this is the transfer function. comes from the DS4432U+ datasheet
    change = fabs((((TPS40305_VFB / BITAXE_RB) - ((vout - TPS40305_VFB) / BITAXE_RA)) / BITAXE_IFS) * 127);
    reg = (uint8_t)ceil(change);

    // Set the MSB high if the requested voltage is BELOW nominal
    if (vout < BITAXE_VNOM)
    {
        reg |= 0x80;
    }

    return reg;
}

bool NerdAxe::initBoard()
{
    Board::initBoard();

    ADC_init();
    SERIAL_init();

    // Init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initializing failed");
        return false;
    }

    EMC2101_init(m_fanInvertPolarity);
    setFanSpeed(m_fanPerc);

    // configure gpios
    gpio_pad_select_gpio(PWR_EN_PIN);
    gpio_set_direction(PWR_EN_PIN, GPIO_MODE_OUTPUT);
    // inverted
    gpio_set_level(PWR_EN_PIN, 1);

    gpio_pad_select_gpio(BM1366_RST_PIN);
    gpio_set_direction(BM1366_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BM1366_RST_PIN, 0);

    return true;
}

void NerdAxe::shutdown() {
    setVoltage(0.0);
}

bool NerdAxe::initAsics()
{
    // disable buck (disables EN pin)
    setVoltage(0.0);

    // set reset low
    gpio_set_level(BM1366_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

     // set output voltage
    setVoltage((float) m_asicVoltageMillis / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // release reset pin
    gpio_set_level(BM1366_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    m_isInitialized = true;
    return true;
}


void NerdAxe::requestBuckTelemtry() {
    // NOP
}

bool NerdAxe::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    if (!core_voltage) {
        // inverted
        gpio_set_level(PWR_EN_PIN, 1);
        return true;
    }
    uint8_t reg_setting = ds4432_tps40305_bitaxe_voltage_to_reg(core_voltage);
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV [0x%02X]", core_voltage, reg_setting);
    DS4432U_set_current_code(0, reg_setting); /// eek!

    // inverted
    gpio_set_level(PWR_EN_PIN, 0);

    return true;
}

void NerdAxe::setFanSpeed(float perc) {
    EMC2101_set_fan_speed(perc);
}

void NerdAxe::setFanPolarity(bool invert) {
    EMC2101_set_fan_polarity(invert);
}

void NerdAxe::getFanSpeed(uint16_t* rpm) {
    *rpm = EMC2101_get_fan_speed();
}

float NerdAxe::getTemperature(int index) {
    if (index > 0) {
        return 0.0f;
    }
    return EMC2101_get_internal_temp() + 5;
}

float NerdAxe::getVRTemp() {
    return 0.0f;
}

float NerdAxe::getVin() {
    return INA260_read_voltage() / 1000.0;
}

float NerdAxe::getIin() {
    return INA260_read_current() / 1000.0;
}

float NerdAxe::getPin() {
    return INA260_read_power() / 1000;
}

float NerdAxe::getVout() {
    return ADC_get_vcore() / 1000.0;
}

float NerdAxe::getIout() {
    return 0.0;
}

float NerdAxe::getPout() {
    return 0.0;
}

bool NerdAxe::selfTest(){
    //Test Core Voltage
    #define CORE_VOLTAGE_TARGET_MIN 1.0 //mV
    #define CORE_VOLTAGE_TARGET_MAX 1.35 //mV

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
    bool powerOK = (power > m_minPin) && (power < m_maxPin);
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
    if(allAsicsDetected) {
        Config::setSelfTest(false);
    }

    return allAsicsDetected;
}
