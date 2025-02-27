// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: NerdAxe2

#ifndef _NERDAXE2_UI_H
#define _NERDAXE2_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t * ui_Splash2;
extern lv_obj_t * ui_Image1;
extern lv_obj_t * ui_lbConnect;
extern lv_obj_t * ui_MiningScreen;
extern lv_obj_t * ui_Image2;
extern lv_obj_t * ui_lbVcore;
extern lv_obj_t * ui_lbPower;
extern lv_obj_t * ui_lbIntensidad;
extern lv_obj_t * ui_lbFan;
extern lv_obj_t * ui_lbEficiency;
extern lv_obj_t * ui_lbTemp;
extern lv_obj_t * ui_lbTime;
extern lv_obj_t * ui_lbIP;
extern lv_obj_t * ui_lbBestDifficulty;
extern lv_obj_t * ui_lbHashrate;
extern lv_obj_t * ui_lbTime1;
extern lv_obj_t * ui_lbASIC;
extern lv_obj_t * ui_SettingsScreen;
extern lv_obj_t * ui_Image4;
extern lv_obj_t * ui_lbIPSet;
extern lv_obj_t * ui_lbVcoreSet;
extern lv_obj_t * ui_lbFreqSet;
extern lv_obj_t * ui_lbFanSet;
extern lv_obj_t * ui_lbPoolSet;
extern lv_obj_t * ui_lbHashrateSet;
extern lv_obj_t * ui_lbShares;
extern lv_obj_t * ui_lbPortSet;
extern lv_obj_t * ui_BTCScreen;
extern lv_obj_t * ui_ImgBTCscreen;
extern lv_obj_t * ui_lblBTCPrice;
extern lv_obj_t * ui_lblPriceInc;
extern lv_obj_t * ui_lblHash;
extern lv_obj_t * ui_lblTemp;
void ui_event_GlobalStats(lv_event_t * e);
extern lv_obj_t * ui_GlobalStats;
extern lv_obj_t * ui_Image5;
extern lv_obj_t * ui_lbVcore1;
extern lv_obj_t * ui_lblBlock;
extern lv_obj_t * ui_lblBlock1;
extern lv_obj_t * ui_lblDifficulty;
extern lv_obj_t * ui_lblGlobalHash;
extern lv_obj_t * ui_lblminFee;
extern lv_obj_t * ui_lblmedFee;
extern lv_obj_t * ui_lblmaxFee;


LV_IMG_DECLARE(ui_img_splashscreen2_png);    // assets\SplashScreen2.png
LV_IMG_DECLARE(ui_img_miningscreen2_png);    // assets\MiningScreen2.png
LV_IMG_DECLARE(ui_img_settingsscreen_png);    // assets\SettingsScreen.png
LV_IMG_DECLARE(ui_img_btcscreen_png);    // assets\BTCScreen.png
LV_IMG_DECLARE(ui_img_globalstats_png);    // assets\globalStats.png


LV_FONT_DECLARE(ui_font_DigitalNumbers16);
LV_FONT_DECLARE(ui_font_DigitalNumbers28);
LV_FONT_DECLARE(ui_font_OpenSansBold13);
LV_FONT_DECLARE(ui_font_OpenSansBold14);
LV_FONT_DECLARE(ui_font_OpenSansBold24);
LV_FONT_DECLARE(ui_font_OpenSansBold45);


void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
