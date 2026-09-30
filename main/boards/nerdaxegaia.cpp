#include <math.h>
#include "serial.h"
#include "board.h"
#include "nvs_config.h"
#include "nerdaxegaia.h"

#include "drivers/nerdaxe/DS4432U.h"
#include "drivers/nerdaxe/EMC2101.h"
#include "drivers/nerdaxe/INA260.h"
#include "drivers/nerdaxe/adc.h"
#include "drivers/nerdaxe/TPS546.h"

#define BM1373_RST_PIN GPIO_NUM_1
#define LDO_EN_PIN GPIO_NUM_12   // LDO enable (active-high) — new Gaia board
#define GAIA_POWER_OFFSET 5

bool tempinit_gaia = false;

static const char* TAG="nerdaxeGaia";

#define MAX(a,b) ((a)>(b)?(a):(b))

NerdaxeGaia::NerdaxeGaia() : NerdAxe() {
    m_deviceModel = "NerdAxeGaia";
    m_miningAgent = "NerdAxe";
    m_asicModel = "BM1373";
    m_version = 200;
    m_asicCount = 1;

    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {300, 320, 340, 350, 360, 380, 400, 420, 440};
    m_asicVoltages = {900, 920, 940, 960, 980, 1000, 1020, 1040};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 940;
    m_absMinAsicVoltageMillis = 800;    // hard floor for core voltage (mV)
    m_absMaxAsicVoltageMillis = 1300;   // hard ceiling for core voltage (mV)
    // m_absMaxAsicFrequency = 750;
    m_initVoltageMillis = 1000;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = false;   // Gaia panel mounted rotated 180° vs NerdAxe → flip default
    m_vr_maxTemp = TPS546_THROTTLE_TEMP; //Set max voltage regulator temp

    m_pidSettings[0].targetTemp = 60;
    m_pidSettings[0].p =  600; // 6.00
    m_pidSettings[0].i =   10; // 0.1
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 40.0;   // web power-gauge reference ceiling (W) — display only, not a runtime cutoff
    m_minPin = 5.0;
    m_maxVin = 13.2;   // 12V input rail (+10%)
    m_minVin = 10.8;   // 12V input rail (-10%)
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 6.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDAXEGAIA
    m_theme = new ThemeNerdaxegaia();
#endif

    // Default web dashboard theme (Nebular) for this board = the Gaia theme.
    // Served to the browser via the API as `defaultTheme`; the web applies it
    // when there is no valid theme saved in localStorage. (This is the web
    // theme, separate from m_theme above, which is the on-device LCD theme.)
    m_defaultTheme = "gaia";

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1373();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}


bool NerdaxeGaia::initBoard()
{
    Board::initBoard();

    // Bring up the LDO FIRST. On the Gaia the TPS546 takes its logic supply
    // from the LDO rail, so it must be powered before ANY TPS546 access
    // (set_vin_config / init below) — otherwise the regulator answers with a
    // garbage device-ID, init fails, and initBoard bails out before the RESET
    // and LDO pins are even configured. Enable it and keep it on continuously
    // (this is exactly what the known-good "GPIO12 jumpered high" does).
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    LDO_enable();
    vTaskDelay(pdMS_TO_TICKS(100));   // let the LDO rail settle before I2C

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

    // 12V input rail: widen the TPS546 VIN thresholds so the input over-voltage
    // fault (5V default = 6V) doesn't trip at 12V. Board-aware — only the Gaia
    // calls this; the shared 5V defaults stay for the NerdAxeGamma. Must run
    // BEFORE TPS546_init() so the correct limits are programmed from the start.
    TPS546_set_vin_config(/*on*/ 10.5f, /*off*/ 9.5f, /*uv_warn*/ 10.5f, /*ov_fault*/ 14.0f);

    // Raise the output over-current protection: the shared driver defaults to
    // 25A warn / 30A fault, which trips around 440MHz @ 1V (~31A). The Gaia's
    // TPS546D24A is rated 40A, so use a conservative 28A warn / 33A fault.
    // Board-specific; must run BEFORE TPS546_init().
    TPS546_set_iout_config(/*warn*/ 28.0f, /*fault*/ 33.0f);

    //Init voltage controller
    if (TPS546_init() != ESP_OK) {
        ESP_LOGE(TAG, "TPS546 init failed!");
        return ESP_FAIL;
    }
    TPS546_set_frequency(400);
    setVoltage(0.0);

    gpio_pad_select_gpio(BM1373_RST_PIN);
    gpio_set_direction(BM1373_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BM1373_RST_PIN, 0);

    // (LDO is already enabled at the top of initBoard, before the TPS546.)

    return true;
}

void NerdaxeGaia::shutdown() {
    setVoltage(0.0);

    // let the core rail collapse before cutting the LDO
    vTaskDelay(pdMS_TO_TICKS(500));

    LDO_disable();

    vTaskDelay(pdMS_TO_TICKS(500));

    Board::shutdown();
}

void NerdaxeGaia::LDO_enable() {
    ESP_LOGI(TAG, "Enable LDO");
    gpio_set_level(LDO_EN_PIN, 1);
}

void NerdaxeGaia::LDO_disable() {
    ESP_LOGI(TAG, "Disable LDO");
    gpio_set_level(LDO_EN_PIN, 0);
}

bool NerdaxeGaia::initAsics() {

    // Core buck off, but keep the LDO ON (already enabled in initBoard and
    // never cycled): the chip runs on the LDO alone during the self-test, so
    // cutting it here — as the old sequence did — left the chip unpowered and
    // it failed to enumerate. This matches the known-good jumpered-high case.
    setVoltage(0.0);
    LDO_enable();   // re-assert (no-op if already high); never disable

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // set reset low
    gpio_set_level(BM1373_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // LDO already on and settled; give the reset a moment before the core ramp
    vTaskDelay(pdMS_TO_TICKS(100));

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    gpio_set_level(BM1373_RST_PIN, 1);

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

bool NerdaxeGaia::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return TPS546_set_vout(core_voltage);
}

float NerdaxeGaia::getTemperature(int index) {

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

float NerdaxeGaia::getVRTemp() {
    //Reading voltage regulator temp
    float vr_temp = TPS546_get_temperature();
    ESP_LOGI(TAG, "Read vr temp = %.3fºC", vr_temp);
    return vr_temp; //- vr_temp (voltage regulator temp)
}

float NerdaxeGaia::getVin() {
    return TPS546_get_vin();
}

float NerdaxeGaia::getIin() {
    float vin = getVin();
    if (!vin) {
        return 0.0f;
    }

    return getPin() / vin;
}

float NerdaxeGaia::getPin() {
    return (TPS546_get_vout() * TPS546_get_iout()) + GAIA_POWER_OFFSET;
}

float NerdaxeGaia::getVout() {
    return ADC_get_vcore() / 1000.0;
}

float NerdaxeGaia::getIout() {
    return TPS546_get_iout();
}

float NerdaxeGaia::getPout() {
    return getPin();
}

