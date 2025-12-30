#include "board.h"
#include "nerdoctaxegamma.h"
#include "nerdqaxeplus2.h"
#include "./drivers/TPS53667.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "nerdoctaxegamma";

NerdOctaxeGamma::NerdOctaxeGamma() : NerdQaxePlus2() {
    m_deviceModel = "NerdOCTAXE-γ";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 8;

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 256;

     // use m_asicVoltage for init
    m_initVoltageMillis = 0;

    m_maxVin = 13.0;
    m_minVin = 11.0;

     // use m_asicVoltage for init
    m_initVoltageMillis = 0;

    m_maxVin = 13.0;
    m_minVin = 11.0;

#ifdef NERDOCTAXEGAMMA
    m_theme = new ThemeNerdoctaxegamma();
#endif

    m_swarmColorName = "#11d51e"; // green

    // Hardware voltage regulator detection (available from rev 3.0+)
    // GPIO3 is located next to GPIO10 (TPS_EN) on the board for easy routing
    // The pin has internal pull-down, so older boards without the strapping
    // resistor will default to TPS53647 (backward compatibility)
    gpio_reset_pin(VR_DETECT_PIN);
    gpio_set_direction(VR_DETECT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(VR_DETECT_PIN, GPIO_PULLDOWN_ONLY);

    // Allow pin to settle after configuration
    vTaskDelay(pdMS_TO_TICKS(1));

    bool isTPS53667 = gpio_get_level(VR_DETECT_PIN);
    m_isTPS53667 = isTPS53667;  // Save for later use in getVRTemp()

    if (isTPS53667) {
        // TPS53667 configuration: 6 phases, 240A capability
        m_numPhases = 6;
        m_imax = 240;        // Current hardware: 24.9kΩ → 240A max (40A per phase with 6 phases)
        m_ifault = 235.0;
        m_maxPin = 300.0;    // ~300W output Typically
        m_minPin = 30.0;
        m_minCurrentA = 0.0f;
        m_maxCurrentA = 25.0f;
        m_tps = new TPS53667();

        // Extended frequency range for TPS53667 (6 phases, higher power capacity)
        m_asicFrequencies = {525, 550, 575, 600, 625, 650, 675, 700, 725, 750, 775, 800};
        m_absMaxAsicFrequency = 850;  // Absolute max for manual input (danger zone)

        // Extended voltage range for TPS53667 (6 phases, higher current capacity)
        m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1210, 1220, 1230, 1240, 1250, 1260};

        // Set higher default values for 6-phase configuration
        m_defaultAsicFrequency = m_asicFrequency = 700;
        m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1210;  // 1.21V

        ESP_LOGI(TAG, "TPS53667 voltage regulator detected (GPIO3=HIGH, 6 phases, 240A max with 24.9kΩ resistor)");
    } else {
        // TPS53647 detected or pin not connected (older revisions)
        // Configure for 4-phase operation (uses inherited frequency limits)
        m_numPhases = 4;
        m_imax = 180;        // 33.2kΩ → 180A max (45A per phase with 4 phases)
        m_ifault = 160.0;
        m_maxPin = 200.0;
        m_minPin = 100.0;
        m_minCurrentA = 0.0f;
        m_maxCurrentA = 20.0f;
        // m_asicFrequencies and m_absMaxAsicFrequency inherited from parent (500-600 MHz, max 800)

        ESP_LOGI(TAG, "TPS53647 voltage regulator detected (GPIO3=LOW, 4 phases, using inherited)");
    }

}

float NerdOctaxeGamma::getVRTemp() {
    // Get temperature from parent implementation
    float vrTemp = NerdQaxePlus::getVRTemp();

    // Apply +8°C offset only for TPS53667 (6 phases) to correct sensor deviation
    if (m_isTPS53667) {
        vrTemp += 8.0f;
    }

    return vrTemp;
}
