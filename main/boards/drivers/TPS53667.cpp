#include "TPS53667.h"
#include "esp_log.h"
#include "pmbus_commands.h"
#include <math.h>

static const char *TAG = "TPS53667";

TPS53667::TPS53667() : TPS53647()
{
    m_i2cAddr = 0x71;
    m_hwMinVoltage = 0.25f;
    m_initVOutMin = 1.005f;
    m_initVOutMax = 1.4f;
    m_initOnOffConfig = 0b00010111;
    m_initOtWarnLimit = 95.0f;
    m_initOtFaultLimit = 125.0f;
}

void TPS53667::set_phases(int num_phases)
{
    TPS53647::set_phases(num_phases);
    write_byte(PMBUS_MFR_SPECIFIC_24, 0x00); // NerEKO: enable all phases
}

// Set up the TPS53667 regulator and turn it on
bool TPS53667::init(int num_phases, int imax, float ifault)
{
    ESP_LOGI(TAG, "Initializing TPS53667 regulator");

    // Establish communication with regulator
    uint16_t device_code = 0;
    read_word(PMBUS_MFR_SPECIFIC_44, &device_code);

    if (device_code != 0x01F8) {
        ESP_LOGE(TAG, "TPS53667 not found. Device code: 0x%04X", device_code);
        return false;
    }

    ESP_LOGI(TAG, "Found TPS53667 controller");

    // clear flags
    write_command(PMBUS_CLEAR_FAULTS);

    // restore all from nvm
    write_command(PMBUS_RESTORE_DEFAULT_ALL);

    // set ON_OFF config, make sure the buck is switched off
    write_byte(PMBUS_ON_OFF_CONFIG, m_initOnOffConfig);

    // Switch frequency, 500kHz
    write_byte(PMBUS_MFR_SPECIFIC_12, 0x20);

    // set maximum current
    write_byte(PMBUS_MFR_SPECIFIC_10, (uint8_t) imax);

    // operation mode VR12 Mode - Enable dynamic phase shedding - Slew Rate 0.68mV/us
    // write_byte(PMBUS_MFR_SPECIFIC_13, 0x89);                      // operation mode VR12 Mode - disable dynamic phase
    // shedding - Slew Rate 0.68mV/us
    write_byte(PMBUS_MFR_SPECIFIC_13, 0x99); // operation mode VR12 Mode - Enable dynamic phase shedding - Slew Rate 0.68mV/us

    set_phases(num_phases);

    write_word(PMBUS_VOUT_MAX, (uint16_t) volt_to_vid(m_initVOutMax)); // to do better ! 1.4V
    // write_byte(PMBUS_MFR_SPECIFIC_00, 0x06);                     // Per-Phase Overcurrent Limit 42A threshold
    write_byte(PMBUS_MFR_SPECIFIC_00, 0x07); // Per-Phase Overcurrent Limit 45A threshold
    // write_byte(PMBUS_MFR_SPECIFIC_16, 0x02);                     // threshold for the VIN Undervoltage UVLO 8.1V threshold
    write_byte(PMBUS_MFR_SPECIFIC_16, 0x01); // threshold for the VIN Undervoltage UVLO 6V threshold
    write_word(PMBUS_MFR_SPECIFIC_19, 0x0003);

    // temperature
    write_word(PMBUS_OT_WARN_LIMIT, float_to_slinear11(m_initOtWarnLimit));
    write_word(PMBUS_OT_FAULT_LIMIT, float_to_slinear11(m_initOtFaultLimit));

    // Iout current
    // set warn and fault to the same value
    write_word(PMBUS_IOUT_OC_WARN_LIMIT, float_to_slinear11(ifault));
    write_word(PMBUS_IOUT_OC_FAULT_LIMIT, float_to_slinear11(ifault));

    // Iin current
    write_word(PMBUS_IIN_OC_WARN_LIMIT, float_to_slinear11(25.0));  // about 300W input power
    write_word(PMBUS_IIN_OC_FAULT_LIMIT, float_to_slinear11(28.0)); // about 336W input power

    m_initialized = true;

    show_voltage_settings();

    return true;
}

void TPS53667::status()
{
    uint8_t status_byte = 0xff;
    uint16_t status_word = 0xffff;
    uint8_t status_vout = 0xff;
    uint8_t status_iout = 0xff;
    uint8_t status_input = 0xff;
    uint8_t status_mfr_specific = 0xff;
    uint8_t status_phase = 0xff;

    read_byte(PMBUS_STATUS_BYTE, &status_byte);
    read_word(PMBUS_STATUS_WORD, &status_word);
    read_byte(PMBUS_STATUS_VOUT, &status_vout);
    read_byte(PMBUS_STATUS_IOUT, &status_iout);
    read_byte(PMBUS_STATUS_INPUT, &status_input);
    read_byte(PMBUS_STATUS_MFR_SPECIFIC, &status_mfr_specific);
    read_byte(PMBUS_MFR_SPECIFIC_24, &status_phase);

    bool isError = (status_byte || status_word || status_vout || status_iout || status_input || status_mfr_specific || status_phase);

    ESP_LOGIE(!isError, TAG,
              "TPS536X7_status  bytes: %02x, word: %04x, vout: %02x, iout: %02x, input: %02x, mfr_spec: %02x phase: %02x",
              status_byte, status_word, status_vout, status_iout, status_input, status_mfr_specific, status_phase);
}