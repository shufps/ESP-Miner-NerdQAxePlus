#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "../displays/displayDriver.h"

const static char* TAG = "board";

Board::Board() {
    m_absMaxAsicFrequency = 0;
    m_absMinAsicVoltageMillis = 0;
    m_absMaxAsicVoltageMillis = 0;
    m_vrFrequency = m_defaultVrFrequency = 0;
    m_hasHashCounter = false;
    m_ecoAsicFrequency = 0;
    m_ecoAsicVoltageMillis = 0;
    m_numFans = 1;
}

// Generic W5500 presence probe (reads VERSIONR over SPI on getEthPins()); false if no eth wiring.
bool Board::isEthConnected()
{
    const EthPins *pins = getEthPins();
    if (!pins || pins->sclk == GPIO_NUM_NC) {
        return false;
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = pins->mosi;
    buscfg.miso_io_num = pins->miso;
    buscfg.sclk_io_num = pins->sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    bool weInitBus = (err == ESP_OK);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "W5500 probe: spi_bus_initialize failed (%s)", esp_err_to_name(err));
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 16;   // W5500 16-bit address phase
    devcfg.address_bits = 8;    // W5500 8-bit control phase
    devcfg.mode = 0;
    devcfg.clock_speed_hz = 2 * 1000 * 1000;
    devcfg.spics_io_num = pins->cs;
    devcfg.queue_size = 1;

    spi_device_handle_t dev = nullptr;
    bool present = false;
    if (spi_bus_add_device(SPI2_HOST, &devcfg, &dev) == ESP_OK) {
        spi_transaction_t t = {};
        t.cmd = 0x0039;                   // VERSIONR address
        t.addr = 0x00;                    // control: common block, read, VDM
        t.length = 8;
        t.rxlength = 8;
        t.flags = SPI_TRANS_USE_RXDATA;
        uint8_t ver = 0;
        if (spi_device_polling_transmit(dev, &t) == ESP_OK) {
            ver = t.rx_data[0];
        }
        ESP_LOGI(TAG, "W5500 interposer probe: VERSIONR=0x%02x -> %s", ver,
                 (ver == 0x04) ? "present" : "absent");
        present = (ver == 0x04);
        spi_bus_remove_device(dev);
    } else {
        ESP_LOGW(TAG, "W5500 probe: spi_bus_add_device failed");
    }

    if (weInitBus) {
        spi_bus_free(SPI2_HOST);
    }
    return present;
}

void Board::loadSettings()
{
    m_fanPerc = Config::getFanSpeed();

    // default values are initialized in the constructor of each board

    // clamp frequency and voltage to absMax values
    if (m_absMaxAsicFrequency) {
        m_asicFrequency = std::min((int) Config::getAsicFrequency(m_asicFrequency), m_absMaxAsicFrequency);
    } else {
        m_asicFrequency = (int) Config::getAsicFrequency(m_asicFrequency);
    }

    if (m_absMaxAsicVoltageMillis) {
        m_asicVoltageMillis = std::min((int) Config::getAsicVoltage(m_asicVoltageMillis), m_absMaxAsicVoltageMillis);
    } else {
        m_asicVoltageMillis = (int) Config::getAsicVoltage(m_asicVoltageMillis);
    }

    m_asicJobIntervalMs = Config::getAsicJobInterval(m_asicJobIntervalMs);
    m_fanInvertPolarity = Config::isFanPolarity(m_fanInvertPolarity);
    m_flipScreen = Config::isFlipScreenEnabled(m_flipScreen);
    m_vrFrequency = Config::getVrFrequency(m_defaultVrFrequency);

    for (int ch = 0; ch < 2; ch++) {
        m_fanMode[ch] = Config::getFanMode(ch, m_fanMode[ch]);
        m_pidSettings[ch].targetTemp = Config::getFanPidTargetTemp(ch, m_pidSettings[ch].targetTemp);
        m_pidSettings[ch].p = Config::getFanPidP(ch, m_pidSettings[ch].p);
        m_pidSettings[ch].i = Config::getFanPidI(ch, m_pidSettings[ch].i);
        m_pidSettings[ch].d = Config::getFanPidD(ch, m_pidSettings[ch].d);
    }

    ESP_LOGI(TAG, "ASIC Frequency: %dMHz", m_asicFrequency);
    ESP_LOGI(TAG, "ASIC voltage: %dmV", m_asicVoltageMillis);
    ESP_LOGI(TAG, "ASIC job interval: %dms", m_asicJobIntervalMs);
    ESP_LOGI(TAG, "invert fan polarity: %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "fan speed: %d%%", (int) m_fanPerc);
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

float Board::getChipTemp(int nr) {
    if (nr < 0 || nr >= m_asicCount) {
        return 0.0f;
    }
    return m_chipTemps[nr];
}

void Board::requestChipTemps() {
    // NOP
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

// requires loadSettings to update the variables
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

// set and get version rolling frequency
// requires loadSettings to update the variables
void Board::setVrFrequency(uint32_t freq) {
    if (!m_asics) {
        return;
    }
    m_asics->setVrFrequency(freq);
}

bool Board::validateVoltage(float core_voltage) {
    if (core_voltage == 0.0f) {
        return true;  // 0V = disable output
    }
    int millis = (int) (core_voltage * 1000.0f);
    // we allow m_absMaxAsicVoltageMillis = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMinAsicVoltageMillis && millis < m_absMinAsicVoltageMillis) {
        ESP_LOGE(TAG, "Validation error. ASIC voltage %d is lower than absolute minimum value %d", millis, m_absMinAsicVoltageMillis);
        return false;
    }
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
