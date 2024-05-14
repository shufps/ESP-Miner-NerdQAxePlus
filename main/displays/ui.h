#ifndef _NERDAXE_UI_H
#define _NERDAXE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *current_screen;

void ui_event_Splash1(lv_event_t * e);
void ui_event_Splash2(lv_event_t * e);
extern lv_obj_t * ui_Splash1;
extern lv_obj_t * ui_Splash2;
extern lv_obj_t * ui_PortalScreen;
extern lv_obj_t * ui_MiningScreen;
extern lv_obj_t * ui_SettingsScreen;
extern lv_obj_t * ui_imgSplash1;
extern lv_obj_t * ui_Image1;
extern lv_obj_t * ui_lbConnect;
extern lv_obj_t * ui_Image2;
extern lv_obj_t * ui_lbVinput;
extern lv_obj_t * ui_lbVcore;
extern lv_obj_t * ui_lbIntensidad;
extern lv_obj_t * ui_lbPower;
extern lv_obj_t * ui_lbEficiency;
extern lv_obj_t * ui_lbTemp;
extern lv_obj_t * ui_lbTime;
extern lv_obj_t * ui_lbIP;
extern lv_obj_t * ui_lbBestDifficulty;
extern lv_obj_t * ui_lbBestDifficultySet;
extern lv_obj_t * ui_lbHashrate;
extern lv_obj_t * ui_lbRPM;
extern lv_obj_t * ui_lbASIC;
extern lv_obj_t * ui_Image4;
extern lv_obj_t * ui_lbIPSet;
extern lv_obj_t * ui_lbVcoreSet;
extern lv_obj_t * ui_lbFreqSet;
extern lv_obj_t * ui_lbFanSet;
extern lv_obj_t * ui_lbPoolSet;
extern lv_obj_t * ui_lbHashrateSet;
extern lv_obj_t * ui_lbShares;
extern lv_obj_t * ui_lbPortSet;



LV_IMG_DECLARE(ui_img_initscreen2_png);    // assets\InitScreen2.png
LV_IMG_DECLARE(ui_img_splashscreen2_png);    // assets\SplashScreen2.png
LV_IMG_DECLARE(ui_img_miningscreen2_png);    // assets\MiningScreen2.png
//LV_IMG_DECLARE(ui_img_PortalScreen_png);    // assets\PortalScreen.png
LV_IMG_DECLARE(ui_img_settingsscreen_png);    // assets\SettingsScreen.png

#define LV_FONT_CUSTOM_DECLARE
LV_FONT_DECLARE(ui_font_DigitalNumbers16);
LV_FONT_DECLARE(ui_font_DigitalNumbers28);
LV_FONT_DECLARE(ui_font_OpenSansBold13);
LV_FONT_DECLARE(ui_font_OpenSansBold14);


#define TDISPLAYS3_LVGL_TICK_PERIOD_MS    30

void ui_init(void);
void changeScreen(void); //* arg);
void manual_lvgl_update();

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
