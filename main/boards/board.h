#pragma once

#include "../displays/images/themes/themes.h"
#include "asic.h"
#include "bm1368.h"
#include "nvs_config.h"

class Board {
  protected:
    // general board information
    const char *m_deviceModel;
    int m_version;
    const char *m_asicModel;
    const char *m_miningAgent;
    int m_asicCount;
    int m_chipsDetected = 0;

    // asic settings
    int m_asicJobIntervalMs;
    float m_asicFrequency;
    float m_asicVoltage;

    // asic difficulty settings
    uint32_t m_asicMinDifficulty;
    uint32_t m_asicMaxDifficulty;

    // Voltage regulator max temperature
    float m_vr_maxTemp = 0.0;

    // fans
    bool m_fanInvertPolarity;
    float m_fanPerc;

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

    virtual bool initBoard() = 0;
    virtual bool initAsics() = 0;

    void loadSettings();
    const char *getDeviceModel();
    const char *getMiningAgent();
    int getVersion();
    const char *getAsicModel();
    int getAsicCount();
    int getAsicJobIntervalMs();
    uint32_t getInitialASICDifficulty();

    // abstract common methos
    virtual bool setVoltage(float core_voltage) = 0;

    virtual void setFanSpeed(float perc) = 0;
    virtual void getFanSpeed(uint16_t *rpm) = 0;

    virtual float readTemperature(int index) = 0;

    virtual float getVin() = 0;
    virtual float getIin() = 0;
    virtual float getPin() = 0;
    virtual float getVout() = 0;
    virtual float getIout() = 0;
    virtual float getPout() = 0;

    virtual void requestBuckTelemtry() = 0;

    virtual void shutdown() = 0;

    virtual bool getPSUFault() {
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

    float getAsicVoltage() {
        return m_asicVoltage;
    }

    float getAsicFrequency() {
        return m_asicFrequency;
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
};
