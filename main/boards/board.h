#pragma once

#include <vector>
#include "../displays/images/themes/themes.h"
#include "asic.h"
#include "bm1368.h"
#include "nvs_config.h"
#include "../pid/PID_v1_bc.h"

enum FanPolarityGuess {
    POLARITY_UNKNOWN,
    POLARITY_NORMAL,
    POLARITY_INVERTED
};

class Board {
  protected:
    // general board information
    const char *m_deviceModel;
    int m_version;
    const char *m_asicModel;
    const char *m_miningAgent;
    int m_asicCount;
    int m_chipsDetected = 0;
    int m_numTempSensors = 0;
    float *m_chipTemps;
    const char *m_swarmColorName = "blue";

    PidSettings m_pidSettings;

    // asic settings
    int m_asicJobIntervalMs;
    int m_asicFrequency;
    int m_asicVoltageMillis;
    int m_absMaxAsicFrequency;
    int m_absMaxAsicVoltageMillis;

    // frequency and voltage options
    std::vector<uint32_t> m_asicFrequencies;
    std::vector<uint32_t> m_asicVoltages;

    // default settings
    int m_defaultAsicFrequency;
    int m_defaultAsicVoltageMillis;

    // asic difficulty settings
    uint32_t m_asicMinDifficulty;
    uint32_t m_asicMaxDifficulty;

    // Voltage regulator max temperature
    float m_vr_maxTemp = 0.0;

    // fans
    static const int FAN_COUNT = 2;
    bool m_fanInvertPolarity;
    bool m_fanAutoPolarity;
    float m_fanPerc[FAN_COUNT];
    bool m_fanEnabled[FAN_COUNT];

    // flip screen
    bool m_flipScreen;

    // max power settings
    float m_maxPin;
    float m_minPin;
    float m_maxVin;
    float m_minVin;

    // display m_theme
    Theme *m_theme = nullptr;

    Asic *m_asics = nullptr;

    bool m_isInitialized = false;

  public:
    Board();

    virtual bool initBoard();
    virtual bool initAsics() = 0;

    void loadSettings();
    const char *getDeviceModel();
    const char *getMiningAgent();
    int getVersion();
    const char *getAsicModel();
    int getAsicCount();
    int getAsicJobIntervalMs();
    uint32_t getInitialASICDifficulty();
    virtual bool setAsicFrequency(float f);

    bool validateFrequency(float frequency);
    bool validateVoltage(float core_voltage);

    // abstract common methos
    virtual bool setVoltage(float core_voltage) = 0;
    virtual void setFanPolarity(bool invert, int fan = 0) = 0;
    virtual void setFanSpeed(float perc, int fan = 0) = 0;
    virtual void getFanSpeed(uint16_t *rpm, int fan = 0) = 0;
    FanPolarityGuess guessFanPolarity();

    virtual float getTemperature(int index) = 0;
    virtual float getVRTemp() = 0;
    virtual bool isPIDAvailable() = 0;

    virtual float getVin() = 0;
    virtual float getIin() = 0;
    virtual float getPin() = 0;
    virtual float getVout() = 0;
    virtual float getIout() = 0;
    virtual float getPout() = 0;

    virtual void requestBuckTelemtry() = 0;

    void setChipTemp(int nr, float temp);
    float getMaxChipTemp();

    virtual void shutdown() = 0;

    virtual bool getPSUFault()
    {
        return false;
    }

    virtual bool selfTest();

    Theme *getTheme()
    {
        return m_theme;
    };

    uint32_t getAsicMaxDifficulty()
    {
        return m_asicMaxDifficulty;
    };

    uint32_t getAsicMinDifficulty()
    {
        return m_asicMinDifficulty;
    };

    bool isInitialized()
    {
        return m_isInitialized;
    };

    virtual Asic *getAsics()
    {
        return m_isInitialized ? m_asics : nullptr;
    }

    int getAsicVoltageMillis()
    {
        return m_asicVoltageMillis;
    }

    int getAsicFrequency()
    {
        return m_asicFrequency;
    }

    int getAbsMaxAsicFrequency() {
        return m_absMaxAsicFrequency;
    }

    int getAbsMaxAsicVoltageMillis() {
        return m_absMaxAsicVoltageMillis;
    }

    int getDefaultAsicVoltageMillis()
    {
        return m_defaultAsicVoltageMillis;
    }

    int getDefaultAsicFrequency()
    {
        return m_defaultAsicFrequency;
    }

    float getMinPin()
    {
        return m_minPin;
    }

    float getMaxPin()
    {
        return m_maxPin;
    }

    float getMinVin()
    {
        return m_minVin;
    }

    float getMaxVin()
    {
        return m_maxVin;
    }

    float getVrMaxTemp()
    {
        return m_vr_maxTemp;
    }

    int getNumTempSensors()
    {
        return m_numTempSensors;
    }

    bool isFlipScreenEnabled()
    {
        return m_flipScreen;
    }

    bool isInvertFanPolarityEnabled()
    {
        return m_fanInvertPolarity;
    }

    bool isAutoFanPolarityEnabled()
    {
        return m_fanAutoPolarity;
    }

    PidSettings *getPidSettings() {
        return &m_pidSettings;
    }

    const std::vector<uint32_t>& getFrequencyOptions() const {
        return m_asicFrequencies;
    }

    const std::vector<uint32_t>& getVoltageOptions() const {
        return m_asicVoltages;
    }

    const char* getSwarmColorName() {
        return m_swarmColorName;
    }

};
