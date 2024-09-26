#pragma once

#include "../displays/images/themes/themes.h"
#include "asic.h"
#include "bm1368.h"
#include "nvs_config.h"

// TODO move this somewhere into the UI code
class Theme {
  protected:
    const lv_img_dsc_t *ui_img_btcscreen;
    const lv_img_dsc_t *ui_img_initscreen;
    const lv_img_dsc_t *ui_img_miningscreen;
    const lv_img_dsc_t *ui_img_portalscreen;
    const lv_img_dsc_t *ui_img_settingscreen;
    const lv_img_dsc_t *ui_img_splashscreen;

  public:
    friend class NerdOctaxePlus;
    friend class NerdQaxePlus;

    const lv_img_dsc_t *getBtcScreen()
    {
        return ui_img_btcscreen;
    };
    const lv_img_dsc_t *getInitScreen()
    {
        return ui_img_initscreen;
    };
    const lv_img_dsc_t *getMiningScreen()
    {
        return ui_img_miningscreen;
    };
    const lv_img_dsc_t *getPortalScreen()
    {
        return ui_img_portalscreen;
    };
    const lv_img_dsc_t *getSettingsScreen()
    {
        return ui_img_settingscreen;
    };
    const lv_img_dsc_t *getSplashScreen()
    {
        return ui_img_splashscreen;
    };
};

class Board {
  protected:
    // general board information
    const char *device_model;
    int version;
    const char *asic_model;
    int asic_count;

    // asic settings
    float asic_job_frequency_ms;
    float asic_frequency;
    float asic_voltage;

    // asic difficulty settings
    uint32_t asic_min_difficulty;
    uint32_t asic_max_difficulty;

    // fans
    bool fan_invert_polarity;
    float fan_perc;

    // display theme
    Theme *theme;

  public:
    Board();

    virtual bool init() = 0;

    void load_settings();
    const char *get_device_model();
    int get_version();
    const char *get_asic_model();
    int get_asic_count();
    double get_asic_job_frequency_ms();
    uint32_t get_initial_ASIC_difficulty();

    // abstract common methos
    virtual bool set_voltage(float core_voltage) = 0;
    virtual uint16_t get_voltage_mv() = 0;

    virtual void set_fan_speed(float perc) = 0;
    virtual void get_fan_speed(uint16_t *rpm) = 0;

    virtual float read_temperature(int index) = 0;

    virtual float get_vin() = 0;
    virtual float get_iin() = 0;
    virtual float get_pin() = 0;
    virtual float get_vout() = 0;
    virtual float get_iout() = 0;
    virtual float get_pout() = 0;

    virtual Asic *getAsics() = 0;

    Theme *getTheme()
    {
        return theme;
    };

    uint32_t getAsicMaxDifficulty()
    {
        return asic_max_difficulty;
    };
    uint32_t getAsicMinDifficulty()
    {
        return asic_min_difficulty;
    };
};
