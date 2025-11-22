#include <math.h>

#include "nvs_flash.h"
#include "esp_log.h"

#define TPS53647_EN_PIN GPIO_NUM_10
#define BM1368_RST_PIN GPIO_NUM_1
#define LDO_EN_PIN GPIO_NUM_13

#include "periodic.hpp"
#include "serial.h"
#include "board.h"
#include "nerdqaxeplus.h"
#include "nvs_config.h"
#include "../displays/displayDriver.h"

#include "EMC2302.h"
#include "TMP1075.h"
#include "TPS53647.h"

#define MAX(a,b) ((a)>(b)?(a):(b))

static const char* TAG="nerdqaxe+";

#define VR_TEMP1075_ADDR   0x1

NerdQaxePlus::NerdQaxePlus() : Board() {
    m_deviceModel = "NerdQAxe+";
    m_miningAgent = m_deviceModel;
    m_version = 501;
    m_asicModel = "BM1368";
    m_asicCount = 4;
    m_asicJobIntervalMs = 1200;
    m_asicFrequencies = {400, 425, 450, 475, 490, 500, 525, 550, 575};
    m_asicVoltages = {1100, 1150, 1200, 1250, 1300, 1350};
    m_defaultAsicFrequency = m_asicFrequency = 490;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1250; // default voltage
    m_absMaxAsicFrequency = 800;
    m_absMaxAsicVoltageMillis = 1400;
    m_initVoltageMillis = 1250;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = false;
    m_numPhases = 2;
    m_imax = m_numPhases * 30;
    m_ifault = (float) (m_imax - 5);

    m_numFans = 2;

    m_maxPin = 70.0;
    m_minPin = 30.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;

    m_pidSettings.targetTemp = 55;
    m_pidSettings.p = 600; //   6.00
    m_pidSettings.i = 10;  //   0.10
    m_pidSettings.d = 1000; // 10.00

    m_asicMaxDifficulty = 1024;
    m_asicMinDifficulty = 256;
    m_asicMinDifficultyDualPool = 128;

#ifdef NERDQAXEPLUS
    m_theme = new ThemeNerdqaxeplus();
#endif
    m_swarmColorName = "#e700d8"; // pink

    m_asics = new BM1368();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();

    m_tps = new TPS53647();
}

bool NerdQaxePlus::initBoard()
{
    Board::initBoard();

    SERIAL_init();

    // Init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initializing failed");
        return false;
    }

    // detect how many TMP1075 we have
    m_numTempSensors = detectNumTempSensors();

    ESP_LOGI(TAG, "found %d ASIC temp measuring sensors", m_numTempSensors);

    EMC2302_init(m_fanInvertPolarity);
    setFanSpeed(m_fanPerc);
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

    vTaskDelay(pdMS_TO_TICKS(500));

    LDO_disable();

    vTaskDelay(pdMS_TO_TICKS(500));
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
    vTaskDelay(pdMS_TO_TICKS(250));

    // enable LDOs
    LDO_enable();

    // wait 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    // init buck and enable output
    m_tps->init(m_numPhases, m_imax, m_ifault);

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // release reset pin
    gpio_set_level(BM1368_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
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

    // set final output voltage
    setVoltage((float) m_asicVoltageMillis / 1000.0f);

    m_isInitialized = true;
    return true;
}


void NerdQaxePlus::requestBuckTelemtry() {
    m_tps->status();
}

void NerdQaxePlus::requestChipTemps() {
    if (!m_asics) {
        return;
    }

    // we need this large interval unfortunately because
    // measuring takes so long
    static Periodic every_15s(sec_to_us(15), /*start_immediately=*/false);
    if (every_15s.due()) {
        m_asics->requestChipTemp();
    }
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
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return m_tps->set_vout(core_voltage);
}

void NerdQaxePlus::setFanSpeedCh(int channel, float perc) {
    EMC2302_set_fan_speed(channel, perc);
}

void NerdQaxePlus::getFanSpeedCh(int channel, uint16_t* rpm) {
    EMC2302_get_fan_speed(channel, rpm);
}

void NerdQaxePlus::setFanPolarity(bool invert) {
    EMC2302_set_fan_polarity(invert);
}

// return the number of asic temp measuring sensors
// skips the VR temp sensor
int NerdQaxePlus::detectNumTempSensors() {
    int found = 0;
    for (int i = 0; i < 4; i++) {
        // don't count the VR sensor on the back
        if (i == VR_TEMP1075_ADDR) {
            continue;
        }
        if (!TMP1075_read_temperature(i)) {
            break;
        }
        ESP_LOGI(TAG, "found asic temp sensor %d", i);
        found++;
    }
    return found;
}

float NerdQaxePlus::getTemperature(int index) {
    if (index >= getNumTempSensors()) {
        return 0.0;
    }

    // read temp and skip index 1
    return TMP1075_read_temperature(index + !!index);
}

float NerdQaxePlus::getVRTemp() {
    float vrTemp = m_tps->get_temperature();

    // test
    float tmp = TMP1075_read_temperature(1);
    ESP_LOGI(TAG, "tmp1075 vs tps: %.2f vs %.2f (diff: %.2f)", tmp, vrTemp, vrTemp - tmp);

    return tmp;
}

float NerdQaxePlus::getVin() {
    return m_tps->get_vin();
}

float NerdQaxePlus::getIin() {
    return m_tps->get_iin();
}

float NerdQaxePlus::getPin() {
    return m_tps->get_pin();
}

float NerdQaxePlus::getVout() {
    return m_tps->get_vout();
}

float NerdQaxePlus::getIout() {
    return m_tps->get_iout();
}

float NerdQaxePlus::getPout() {
    return m_tps->get_pout();
}

Board::Error NerdQaxePlus::getFault(uint32_t *status) {
    *status = 0x00000000;

    uint8_t status_byte = m_tps->get_status_byte();
    uint8_t status_iout = m_tps->get_status_iout();
    uint8_t status_vout = m_tps->get_status_vout();
    uint8_t status_input = m_tps->get_status_input();
    uint8_t status_temp = m_tps->get_status_temp();

    *status = (static_cast<uint32_t>(status_byte) << 24) |
              (static_cast<uint32_t>(status_iout) << 16) |
              (static_cast<uint32_t>(status_vout) << 8)  |
              (static_cast<uint32_t>(status_input));

    // If +12V is missing, the PMBus device does not respond to I2C reads,
    // resulting in all bytes being 0xFF due to no ACK.
    // The combined && check ensures we only flag a PSU fault when *all*
    // reads failed, avoiding false triggers from single read errors.
    if (status_byte == 0xff &&
        status_iout == 0xff &&
        status_vout == 0xff &&
        status_temp == 0xff &&
        status_input == 0xff) {
        return Board::Error::PSU_FAULT;
    }

    // Check for output overcurrent fault flag
    // Bit 7: IOUT_OCF
    if (status_iout != 0xff && (status_iout & 0x80)) {
        return Board::Error::IOUT_OC_FAULT;
    }

    // Check for output voltage fault flags
    // Bit 7: VOUT_OVF, Bit 4: VOUT_UVF
    if (status_vout != 0xff && (status_vout & 0x90)) {
        return Board::Error::VOUT_FAULT;
    }

    // Check for overtemperature fault flag
    // Bit 7: OTF
    if (status_temp != 0xff && (status_temp & 0x80)) {
        return Board::Error::VREG_TEMP_FAULT;
    }

    // Check for PSU-level input or state faults
    // status_input: Bit 7 = VIN_OVF, Bit 4 = VIN_UVF, Bit 2 = IIN_OCF
    if (status_input != 0xff && (status_input & 0x94)) {
        return Board::Error::PSU_FAULT;
    }

    // is buck off? Then something is wrong ...
    // return general error.
    // status_byte: Bit 6 = OFF
    if (status_byte != 0xff && (status_byte & 0x40)) {
        return Board::Error::PSU_FAULT;
    }

    return Board::Error::NONE;
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

    return true;
}
