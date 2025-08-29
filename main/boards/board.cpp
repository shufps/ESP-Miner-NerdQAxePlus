#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "../displays/displayDriver.h"

const static char* TAG = "board";

Board::Board() {
    m_fanAutoPolarity = true; // default detect polarity
    m_absMaxAsicFrequency = 0;
    m_absMaxAsicVoltageMillis = 0;
}

void Board::loadSettings()
{
    m_fanPerc[0] = Config::getFanSpeed();
    m_fanPerc[1] = Config::getFan2Speed();
    m_fanEnabled[0] = true;
    m_fanEnabled[1] = Config::isFan2Enabled();

    // default values are initialized in the constructor of each board
    m_asicFrequency = Config::getAsicFrequency(m_asicFrequency);
    m_asicVoltageMillis = Config::getAsicVoltage(m_asicVoltageMillis);
    m_asicJobIntervalMs = Config::getAsicJobInterval(m_asicJobIntervalMs);
    m_fanInvertPolarity = Config::isInvertFanPolarityEnabled(m_fanInvertPolarity);
    m_fanAutoPolarity = Config::isAutoFanPolarityEnabled(m_fanAutoPolarity);
    m_flipScreen = Config::isFlipScreenEnabled(m_flipScreen);

    m_pidSettings.targetTemp = Config::getPidTargetTemp(m_pidSettings.targetTemp);
    m_pidSettings.p = Config::getPidP(m_pidSettings.p);
    m_pidSettings.i = Config::getPidI(m_pidSettings.i);
    m_pidSettings.d = Config::getPidD(m_pidSettings.d);

    ESP_LOGI(TAG, "ASIC Frequency: %dMHz", m_asicFrequency);
    ESP_LOGI(TAG, "ASIC voltage: %dmV", m_asicVoltageMillis);
    ESP_LOGI(TAG, "ASIC job interval: %dms", m_asicJobIntervalMs);
    ESP_LOGI(TAG, "invert fan polarity: %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "fan1 speed: %d%%", (int) m_fanPerc[0]);
    ESP_LOGI(TAG, "fan2 speed: %d%% (enabled: %s)", (int) m_fanPerc[1], m_fanEnabled[1] ? "true" : "false");
}

bool Board::initBoard() {
    m_chipTemps = new float[m_asicCount]();
    return true;
}

void Board::setChipTemp(int nr, float temp) {
    if (nr < 0 || nr >= m_asicCount) {
        return;
    }
    m_chipTemps[nr] = temp;
}

float Board::getMaxChipTemp() {
    float maxTemp = 0.0f;
    for (int i=0;i<m_asicCount;i++) {
        maxTemp = std::max(maxTemp, m_chipTemps[i]);
    }
    return maxTemp;
}

const char *Board::getDeviceModel()
{
    return m_deviceModel;
}

const char *Board::getMiningAgent()
{
    return m_miningAgent;
}

int Board::getVersion()
{
    return m_version;
}

const char *Board::getAsicModel()
{
    return m_asicModel;
}

int Board::getAsicCount()
{
    return m_asicCount;
}

int Board::getAsicJobIntervalMs()
{
    return m_asicJobIntervalMs;
}

bool Board::selfTest(){

    // Initialize the display
    DisplayDriver *temp_display;
    temp_display = new DisplayDriver();
    temp_display->init(this);

    temp_display->logMessage("Self test not supported on this board...");
    ESP_LOGI("board", "Self test not supported on this board");
    vTaskDelay(pdMS_TO_TICKS(1000));

    return false;
}

FanPolarityGuess Board::guessFanPolarity() {
    const int settleTimeMs = 2000;
    const float lowPWM = 0.40f;
    const float highPWM = 0.60f;
    const float similarityThreshold = 0.90f; // ≥90% match = too similar to tell

    uint16_t rpmLow = 0, rpmHigh = 0;

    // bring it to run at a safe setting
    ESP_LOGI("polarity", "set 50%%");
    setFanPolarity(false, 0);
    setFanSpeed(0.5f, 0);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));

    // Test low speed
    setFanSpeed(lowPWM, 0);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));
    getFanSpeed(&rpmLow, 0);
    ESP_LOGI("polarity", "set %.2f%% read: %d", lowPWM, rpmLow);

    // Test high speed
    setFanSpeed(highPWM, 0);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));
    getFanSpeed(&rpmHigh, 0);
    ESP_LOGI("polarity", "set %.2f%% read: %d", highPWM, rpmHigh);

    // Reset to mid-range to be safe
    ESP_LOGI("polarity", "set 50%%");
    setFanSpeed(0.5f, 0);

    // No signal at all? Can't tell.
    if (rpmLow == 0 && rpmHigh == 0) {
        ESP_LOGW("polarity", "unknown fan polarity!");
        return POLARITY_UNKNOWN;
    }

    // Calculate similarity
    uint16_t minRPM = std::min(rpmLow, rpmHigh);
    uint16_t maxRPM = std::max(rpmLow, rpmHigh);

    float similarity = (float)minRPM / (maxRPM + 1);  // avoid div by zero

    // Too close? Can't decide
    if (similarity > similarityThreshold) {
        ESP_LOGW("polarity", "RPM difference too little, unknown fan polarity!");
        return POLARITY_UNKNOWN;
    }

    // Now decide
    if (rpmHigh > rpmLow) {
        ESP_LOGI("polarity", "normal fan polarity detected");
        return POLARITY_NORMAL;
    } else {
        ESP_LOGI("polarity", "inverted fan polarity detected");
        return POLARITY_INVERTED;
    }
}

bool Board::validateVoltage(float core_voltage) {
    int millis = (int) (core_voltage * 1000.0f);
    // we allow m_absMaxAsicVoltageMillis = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMaxAsicVoltageMillis && millis > m_absMaxAsicVoltageMillis) {
        ESP_LOGE(TAG, "Validation error. ASIC voltage %d is higher than absolute maximum value %d", millis, m_absMaxAsicVoltageMillis);
        return false;
    }
    return true;
}

bool Board::validateFrequency(float frequency) {
    // we allow m_absMaxAsicFrequency = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMaxAsicFrequency && frequency > (float) m_absMaxAsicFrequency) {
        ESP_LOGE(TAG, "Validation error. ASIC Frequency %.3f is higher than absolute maximum value %.3f", frequency, (float) m_absMaxAsicFrequency);
        return false;
    }
    return true;
}

bool Board::setAsicFrequency(float frequency) {
    if (!validateFrequency(frequency)) {
        return false;
    }

    // not initialized
    if (!m_asics) {
        return false;
    }

    return m_asics->setAsicFrequency(frequency);
}
