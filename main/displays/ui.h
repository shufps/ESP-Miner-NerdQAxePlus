#pragma once

#include "lvgl.h"
#include "../boards/board.h"
#include "../otp/qrcodegen.h"


LV_IMG_DECLARE(ui_img_overheat_png);      // overheating screen
LV_IMG_DECLARE(ui_img_safe_png);

#define LV_FONT_CUSTOM_DECLARE
LV_FONT_DECLARE(ui_font_DigitalNumbers16);
LV_FONT_DECLARE(ui_font_DigitalNumbers28);
LV_FONT_DECLARE(ui_font_OpenSansBold13);
LV_FONT_DECLARE(ui_font_OpenSansBold14);
LV_FONT_DECLARE(ui_font_OpenSansBold45);
LV_FONT_DECLARE(ui_font_OpenSansBold24);
LV_FONT_DECLARE(ui_font_vt323_35);
LV_FONT_DECLARE(ui_font_vt323_21);

#define TDISPLAYS3_LVGL_TICK_PERIOD_MS 30

LV_IMG_DECLARE(ui_img_found_block_png);

class DisplayDriver;

class UI {
protected:
    lv_obj_t *ui_Splash1 = nullptr;
    lv_obj_t *ui_Splash2 = nullptr;
    lv_obj_t *ui_PortalScreen = nullptr;
    lv_obj_t *ui_MiningScreen = nullptr;
    lv_obj_t *ui_SettingsScreen = nullptr;
    lv_obj_t *ui_lbSSID = nullptr;
    lv_obj_t *ui_imgSplash1 = nullptr;
    lv_obj_t *ui_Image1 = nullptr;
    lv_obj_t *ui_lbConnect = nullptr;
    lv_obj_t *ui_Image2 = nullptr;
    lv_obj_t *ui_lbVinput = nullptr;
    lv_obj_t *ui_lbVcore = nullptr;
    lv_obj_t *ui_lbIntensidad = nullptr;
    lv_obj_t *ui_lbPower = nullptr;
    lv_obj_t *ui_lbEficiency = nullptr;
    lv_obj_t *ui_lbTemp = nullptr;
    lv_obj_t *ui_lbTime = nullptr;
    lv_obj_t *ui_lbIP = nullptr;
    lv_obj_t *ui_lbBestDifficulty = nullptr;
    lv_obj_t *ui_lbBestDifficultySet = nullptr;
    lv_obj_t *ui_lbHashrate = nullptr;
    lv_obj_t *ui_lbRPM = nullptr;
    lv_obj_t *ui_lbASIC = nullptr;
    lv_obj_t *ui_Image4 = nullptr;
    lv_obj_t *ui_lbIPSet = nullptr;
    lv_obj_t *ui_lbVcoreSet = nullptr;
    lv_obj_t *ui_lbFreqSet = nullptr;
    lv_obj_t *ui_lbFanSet = nullptr;
    lv_obj_t *ui_lbPoolSet = nullptr;
    lv_obj_t *ui_lbHashrateSet = nullptr;
    lv_obj_t *ui_lbShares = nullptr;
    lv_obj_t *ui_lbPortSet = nullptr;
    lv_obj_t *ui_LogScreen = nullptr;
    lv_obj_t *ui_LogLabel = nullptr;
    lv_obj_t *ui_BTCScreen = nullptr;
    lv_obj_t *ui_ImgBTCscreen = nullptr;
    lv_obj_t *ui_lblBTCPrice = nullptr;
    lv_obj_t *ui_lblPriceInc = nullptr;
    lv_obj_t *ui_lblHashPrice = nullptr;
    lv_obj_t *ui_lblTempPrice = nullptr;
    lv_obj_t *ui_errOverlayContainer = nullptr;
    lv_obj_t *ui_imageOverlayContainer = nullptr;
    lv_obj_t *ui_GlobalStats = nullptr;
    lv_obj_t *ui_Image5 = nullptr;
    lv_obj_t *ui_lblHalvingPercent = nullptr;
    lv_obj_t *ui_lblBlock = nullptr;
    lv_obj_t *ui_lblBlocksToHalving = nullptr;
    lv_obj_t *ui_lblDifficulty = nullptr;
    lv_obj_t *ui_lblGlobalHash = nullptr;
    lv_obj_t *ui_lbllowFee = nullptr;
    lv_obj_t *ui_lblmedFee = nullptr;
    lv_obj_t *ui_lblhighFee = nullptr;
    lv_obj_t *ui_qrScreen = nullptr;
    lv_obj_t *ui_PowerOffScreen = nullptr;

    Board* m_board;
    Theme* m_theme;

    lv_color_t* m_qr_canvas_buf = nullptr;
    int         m_qr_canvas_w   = 0;   // == height
    lv_obj_t*   m_qr_canvas     = nullptr;

    int64_t m_last_screen_change_time;

    void changeScreen(void);
    void manual_lvgl_update();

    void showImageOverlay(const lv_img_dsc_t *img);
    void hideImageOverlay();

    // QR
    void createQRScreen(uint8_t *buf, int size);
    void destroyQRScreen();
public:
    UI();

    void init(Board* board);

    void miningScreenInit(void);
    void settingsScreenInit(void);
    void splash1ScreenInit(void);
    void splash2ScreenInit(void);
    void portalScreenInit(void);
    void logScreenInit(void);
    void bTCScreenInit(void);
    void globalStatsScreenInit(void);
    void powerOffScreenInit(void);

    void showErrorOverlay(const char *error_message, uint32_t error_code);
    void hideErrorOverlay();

    friend class DisplayDriver;
};

