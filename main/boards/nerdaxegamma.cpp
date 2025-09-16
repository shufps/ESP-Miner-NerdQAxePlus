#include <math.h>
#include "serial.h"
#include "board.h"
#include "nvs_config.h"
#include "nerdaxegamma.h"

#include "drivers/nerdaxe/DS4432U.h"
#include "drivers/nerdaxe/EMC2101.h"
#include "drivers/nerdaxe/INA260.h"
#include "drivers/nerdaxe/adc.h"
#include "drivers/nerdaxe/TPS546.h"

#define BM1370_RST_PIN GPIO_NUM_1
#define GAMMA_POWER_OFFSET 5

bool tempinit = false;

static const char* TAG="nerdaxeGamma";

#define MAX(a,b) ((a)>(b)?(a):(b))

NerdaxeGamma::NerdaxeGamma() : NerdAxe() {
    m_deviceModel = "NerdAxeGamma";
    m_miningAgent = "NerdAxe";
    m_asicModel = "BM1370";
    m_version = 200;
    m_asicCount = 1;

    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {500, 515, 525, 550, 575};
    m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 515;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150;
    m_initVoltageMillis = 1150;
    m_fanInvertPolarity = true;
    m_fanPerc = 100;
    m_flipScreen = true;
    m_vr_maxTemp = TPS546_THROTTLE_TEMP; //Set max voltage regulator temp

    m_pidSettings.targetTemp = 60;
    m_pidSettings.p =  600; // 6.00
    m_pidSettings.i =   10; // 0.1
    m_pidSettings.d = 1000; // 10.00

    m_maxPin = 25.0;
    m_minPin = 5.0;
    m_maxVin = 5.5;
    m_minVin = 4.5;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;

#ifdef NERDAXEGAMMA
    m_theme = new ThemeNerdaxegamma();
#endif

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1370();
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}


bool NerdaxeGamma::initBoard()
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
    EMC2101_set_ideality_factor(EMC2101_IDEALITY_1_0319);
    EMC2101_set_beta_compensation(EMC2101_BETA_11);
    setFanSpeed(m_fanPerc);

    //Init voltage controller
    if (TPS546_init() != ESP_OK) {
        ESP_LOGE(TAG, "TPS546 init failed!");
        return ESP_FAIL;
    }
    setVoltage(0.0);

    gpio_pad_select_gpio(BM1370_RST_PIN);
    gpio_set_direction(BM1370_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BM1370_RST_PIN, 0);

    return true;
}

void NerdaxeGamma::shutdown() {
    setVoltage(0.0);
}

bool NerdaxeGamma::initAsics() {

    // set output voltage
    setVoltage(0.0);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // set reset low
    gpio_set_level(BM1370_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // release reset pin
    gpio_set_level(BM1370_RST_PIN, 1);

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

    m_isInitialized = true;
    return true;
}

bool NerdaxeGamma::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return TPS546_set_vout(core_voltage);
}

float NerdaxeGamma::getTemperature(int index) {

    if (!m_isInitialized) {
        return EMC2101_get_internal_temp() + 5;
    }

    if (index > 0) {
        return 0.0f;
    }

    //Reading ASIC temp
    float asic_temp = EMC2101_get_external_temp();
    ESP_LOGI(TAG, "Read ASIC temp = %.3fºC", asic_temp);
    return asic_temp; //External board Temp
}

float NerdaxeGamma::getVRTemp() {
    //Reading voltage regulator temp
    float vr_temp = TPS546_get_temperature();
    ESP_LOGI(TAG, "Read vr temp = %.3fºC", vr_temp);
    return vr_temp; //- vr_temp (voltage regulator temp)
}

float NerdaxeGamma::getVin() {
    return TPS546_get_vin();
}

float NerdaxeGamma::getIin() {
    return TPS546_get_iout();
}

float NerdaxeGamma::getPin() {
    return (TPS546_get_vout() * TPS546_get_iout()) + GAMMA_POWER_OFFSET;
}

float NerdaxeGamma::getVout() {
    return ADC_get_vcore() / 1000.0;
}

float NerdaxeGamma::getIout() {
    return TPS546_get_iout();
}

float NerdaxeGamma::getPout() {
    return getPin();
}

