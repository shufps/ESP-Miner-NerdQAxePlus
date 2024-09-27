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
    int m_asicCount;

    // asic settings
    float m_asicJobIntervalMs;
    float m_asicFrequency;
    float m_asicVoltage;

    // asic difficulty settings
    uint32_t m_asicMinDifficulty;
    uint32_t m_asicMaxDifficulty;

    // fans
    bool m_fanInvertPolarity;
    float m_fanPerc;

    // display m_theme
    Theme *m_theme;

  public:
    Board();

    virtual bool init() = 0;

    void loadSettings();
    const char *getDeviceModel();
    int getVersion();
    const char *getAsicModel();
    int getAsicCount();
    double getAsicJobIntervalMs();
    uint32_t getInitialASICDifficulty();

    // abstract common methos
    virtual bool setVoltage(float core_voltage) = 0;
    virtual uint16_t getVoltageMv() = 0;

    virtual void setFanSpeed(float perc) = 0;
    virtual void getFanSpeed(uint16_t *rpm) = 0;

    virtual float readTemperature(int index) = 0;

    virtual float getVin() = 0;
    virtual float getIin() = 0;
    virtual float getPin() = 0;
    virtual float getVout() = 0;
    virtual float getIout() = 0;
    virtual float getPout() = 0;

    virtual Asic *getAsics() = 0;

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
};
