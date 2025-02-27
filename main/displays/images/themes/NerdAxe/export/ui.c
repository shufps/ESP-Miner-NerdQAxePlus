// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: NerdAxe2

#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_Splash2;
lv_obj_t * ui_Image1;
lv_obj_t * ui_lbConnect;
lv_obj_t * ui_MiningScreen;
lv_obj_t * ui_Image2;
lv_obj_t * ui_lbVcore;
lv_obj_t * ui_lbPower;
lv_obj_t * ui_lbIntensidad;
lv_obj_t * ui_lbFan;
lv_obj_t * ui_lbEficiency;
lv_obj_t * ui_lbTemp;
lv_obj_t * ui_lbTime;
lv_obj_t * ui_lbIP;
lv_obj_t * ui_lbBestDifficulty;
lv_obj_t * ui_lbHashrate;
lv_obj_t * ui_lbTime1;
lv_obj_t * ui_lbASIC;
lv_obj_t * ui_SettingsScreen;
lv_obj_t * ui_Image4;
lv_obj_t * ui_lbIPSet;
lv_obj_t * ui_lbVcoreSet;
lv_obj_t * ui_lbFreqSet;
lv_obj_t * ui_lbFanSet;
lv_obj_t * ui_lbPoolSet;
lv_obj_t * ui_lbHashrateSet;
lv_obj_t * ui_lbShares;
lv_obj_t * ui_lbPortSet;
lv_obj_t * ui_BTCScreen;
lv_obj_t * ui_ImgBTCscreen;
lv_obj_t * ui_lblBTCPrice;
lv_obj_t * ui_lblPriceInc;
lv_obj_t * ui_lblHash;
lv_obj_t * ui_lblTemp;
void ui_event_GlobalStats(lv_event_t * e);
lv_obj_t * ui_GlobalStats;
lv_obj_t * ui_Image5;
lv_obj_t * ui_lbVcore1;
lv_obj_t * ui_lblBlock;
lv_obj_t * ui_lblBlock1;
lv_obj_t * ui_lblDifficulty;
lv_obj_t * ui_lblGlobalHash;
lv_obj_t * ui_lblminFee;
lv_obj_t * ui_lblmedFee;
lv_obj_t * ui_lblmaxFee;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=1
    #error "LV_COLOR_16_SWAP should be 1 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_GlobalStats(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_SCREEN_LOADED) {
        _ui_screen_change(ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 1500);
    }
}

///////////////////// SCREENS ////////////////////
void ui_Splash2_screen_init(void)
{
    ui_Splash2 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Splash2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image1 = lv_img_create(ui_Splash2);
    lv_img_set_src(ui_Image1, &ui_img_splashscreen2_png);
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lbConnect = lv_label_create(ui_Splash2);
    lv_obj_set_width(ui_lbConnect, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbConnect, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbConnect, -31);
    lv_obj_set_y(ui_lbConnect, -40);
    lv_obj_set_align(ui_lbConnect, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbConnect, "mySSID");
    lv_obj_set_style_text_color(ui_lbConnect, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbConnect, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbConnect, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbConnect, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

}
void ui_MiningScreen_screen_init(void)
{
    ui_MiningScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MiningScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image2 = lv_img_create(ui_MiningScreen);
    lv_img_set_src(ui_Image2, &ui_img_miningscreen2_png);
    lv_obj_set_width(ui_Image2, LV_SIZE_CONTENT);   /// 320
    lv_obj_set_height(ui_Image2, LV_SIZE_CONTENT);    /// 170
    lv_obj_set_align(ui_Image2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lbVcore = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbVcore, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbVcore, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbVcore, 234);
    lv_obj_set_y(ui_lbVcore, -34);
    lv_obj_set_align(ui_lbVcore, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVcore, "1200mV");
    lv_obj_set_style_text_color(ui_lbVcore, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcore, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcore, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcore, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPower = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbPower, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbPower, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbPower, 234);
    lv_obj_set_y(ui_lbPower, -12);
    lv_obj_set_align(ui_lbPower, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPower, "12,2W");
    lv_obj_set_style_text_color(ui_lbPower, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPower, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPower, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPower, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbIntensidad = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbIntensidad, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbIntensidad, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbIntensidad, 234);
    lv_obj_set_y(ui_lbIntensidad, 10);
    lv_obj_set_align(ui_lbIntensidad, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbIntensidad, "2.344mA");
    lv_obj_set_style_text_color(ui_lbIntensidad, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIntensidad, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIntensidad, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIntensidad, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbFan = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbFan, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbFan, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbFan, 234);
    lv_obj_set_y(ui_lbFan, 32);
    lv_obj_set_align(ui_lbFan, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbFan, "0rpm");
    lv_obj_set_style_text_color(ui_lbFan, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbFan, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbFan, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbFan, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbEficiency = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbEficiency, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbEficiency, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbEficiency, -43);
    lv_obj_set_y(ui_lbEficiency, 61);
    lv_obj_set_align(ui_lbEficiency, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbEficiency, "12.4");
    lv_obj_set_style_text_color(ui_lbEficiency, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbEficiency, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbEficiency, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbEficiency, &ui_font_DigitalNumbers16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbTemp = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbTemp, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbTemp, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbTemp, -139);
    lv_obj_set_y(ui_lbTemp, 24);
    lv_obj_set_align(ui_lbTemp, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbTemp, "48");
    lv_obj_set_style_text_color(ui_lbTemp, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbTemp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbTemp, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbTemp, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbTime = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbTime, -190);
    lv_obj_set_y(ui_lbTime, 0);
    lv_obj_set_align(ui_lbTime, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbTime, "1d 2h 5m");
    lv_obj_set_style_text_color(ui_lbTime, lv_color_hex(0xDEEE00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbTime, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbTime, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbTime, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbIP = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbIP, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbIP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbIP, -16);
    lv_obj_set_y(ui_lbIP, -77);
    lv_obj_set_align(ui_lbIP, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbIP, "192.168.1.200");
    lv_obj_set_style_text_color(ui_lbIP, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIP, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIP, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIP, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbBestDifficulty = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbBestDifficulty, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbBestDifficulty, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbBestDifficulty, 34);
    lv_obj_set_y(ui_lbBestDifficulty, 21);
    lv_obj_set_align(ui_lbBestDifficulty, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbBestDifficulty, "22M");
    lv_obj_set_style_text_color(ui_lbBestDifficulty, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbBestDifficulty, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbBestDifficulty, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbBestDifficulty, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbHashrate = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbHashrate, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbHashrate, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbHashrate, -208);
    lv_obj_set_y(ui_lbHashrate, 59);
    lv_obj_set_align(ui_lbHashrate, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbHashrate, "500,0");
    lv_obj_set_style_text_color(ui_lbHashrate, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbHashrate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbHashrate, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbHashrate, &ui_font_DigitalNumbers28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbTime1 = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbTime1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbTime1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbTime1, 20);
    lv_obj_set_y(ui_lbTime1, -9);
    lv_obj_set_align(ui_lbTime1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbTime1, "5000");
    lv_obj_set_style_text_color(ui_lbTime1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbTime1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbTime1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbTime1, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbASIC = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbASIC, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbASIC, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbASIC, 111);
    lv_obj_set_y(ui_lbASIC, -66);
    lv_obj_set_align(ui_lbASIC, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbASIC, "BM1366");
    lv_obj_set_style_text_color(ui_lbASIC, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbASIC, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbASIC, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbASIC, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

}
void ui_SettingsScreen_screen_init(void)
{
    ui_SettingsScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SettingsScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image4 = lv_img_create(ui_SettingsScreen);
    lv_img_set_src(ui_Image4, &ui_img_settingsscreen_png);
    lv_obj_set_width(ui_Image4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Image4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Image4, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image4, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lbIPSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbIPSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbIPSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbIPSet, -16);
    lv_obj_set_y(ui_lbIPSet, -77);
    lv_obj_set_align(ui_lbIPSet, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbIPSet, "192.168.1.200");
    lv_obj_set_style_text_color(ui_lbIPSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIPSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIPSet, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIPSet, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbVcoreSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbVcoreSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbVcoreSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbVcoreSet, 43);
    lv_obj_set_y(ui_lbVcoreSet, -45);
    lv_obj_set_align(ui_lbVcoreSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVcoreSet, "1200mV");
    lv_obj_set_style_text_color(ui_lbVcoreSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcoreSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcoreSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcoreSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbFreqSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbFreqSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbFreqSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbFreqSet, 43);
    lv_obj_set_y(ui_lbFreqSet, -25);
    lv_obj_set_align(ui_lbFreqSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbFreqSet, "485");
    lv_obj_set_style_text_color(ui_lbFreqSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbFreqSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbFreqSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbFreqSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbFanSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbFanSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbFanSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbFanSet, 43);
    lv_obj_set_y(ui_lbFanSet, -5);
    lv_obj_set_align(ui_lbFanSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbFanSet, "AUTO");
    lv_obj_set_style_text_color(ui_lbFanSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbFanSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbFanSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbFanSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPoolSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbPoolSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbPoolSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbPoolSet, 169);
    lv_obj_set_y(ui_lbPoolSet, -9);
    lv_obj_set_align(ui_lbPoolSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPoolSet, "public-pool.io");
    lv_obj_set_style_text_color(ui_lbPoolSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPoolSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPoolSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPoolSet, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbHashrateSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbHashrateSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbHashrateSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbHashrateSet, -208);
    lv_obj_set_y(ui_lbHashrateSet, 59);
    lv_obj_set_align(ui_lbHashrateSet, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbHashrateSet, "500,0");
    lv_obj_set_style_text_color(ui_lbHashrateSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbHashrateSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbHashrateSet, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbHashrateSet, &ui_font_DigitalNumbers28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbShares = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbShares, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbShares, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbShares, -23);
    lv_obj_set_y(ui_lbShares, 58);
    lv_obj_set_align(ui_lbShares, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbShares, "0/0");
    lv_obj_set_style_text_color(ui_lbShares, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbShares, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbShares, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbShares, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPortSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbPortSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbPortSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbPortSet, 211);
    lv_obj_set_y(ui_lbPortSet, 13);
    lv_obj_set_align(ui_lbPortSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPortSet, "3333");
    lv_obj_set_style_text_color(ui_lbPortSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPortSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPortSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPortSet, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

}
void ui_BTCScreen_screen_init(void)
{
    ui_BTCScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_BTCScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BTCScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BTCScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ImgBTCscreen = lv_img_create(ui_BTCScreen);
    lv_img_set_src(ui_ImgBTCscreen, &ui_img_btcscreen_png);
    lv_obj_set_width(ui_ImgBTCscreen, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ImgBTCscreen, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_ImgBTCscreen, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImgBTCscreen, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_ImgBTCscreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lblBTCPrice = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblBTCPrice, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblBTCPrice, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblBTCPrice, 30);
    lv_obj_set_y(ui_lblBTCPrice, 47);
    lv_obj_set_align(ui_lblBTCPrice, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblBTCPrice, "62523$");
    lv_obj_set_style_text_color(ui_lblBTCPrice, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBTCPrice, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBTCPrice, &ui_font_OpenSansBold45, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblPriceInc = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblPriceInc, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblPriceInc, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblPriceInc, 193);
    lv_obj_set_y(ui_lblPriceInc, 49);
    lv_obj_set_align(ui_lblPriceInc, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblPriceInc, "2%");
    lv_obj_set_style_text_color(ui_lblPriceInc, lv_color_hex(0x07FF2A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblPriceInc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblPriceInc, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHash = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblHash, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblHash, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblHash, 236);
    lv_obj_set_y(ui_lblHash, -63);
    lv_obj_set_align(ui_lblHash, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblHash, "500,0");
    lv_obj_set_style_text_color(ui_lblHash, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHash, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHash, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHash, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblTemp = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblTemp, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblTemp, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblTemp, 261);
    lv_obj_set_y(ui_lblTemp, -18);
    lv_obj_set_align(ui_lblTemp, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblTemp, "24");
    lv_obj_set_style_text_color(ui_lblTemp, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblTemp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblTemp, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblTemp, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

}
void ui_GlobalStats_screen_init(void)
{
    ui_GlobalStats = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_GlobalStats, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image5 = lv_img_create(ui_GlobalStats);
    lv_img_set_src(ui_Image5, &ui_img_globalstats_png);
    lv_obj_set_width(ui_Image5, LV_SIZE_CONTENT);   /// 321
    lv_obj_set_height(ui_Image5, LV_SIZE_CONTENT);    /// 170
    lv_obj_set_align(ui_Image5, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image5, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image5, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lbVcore1 = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lbVcore1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbVcore1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbVcore1, -64);
    lv_obj_set_y(ui_lbVcore1, 36);
    lv_obj_set_align(ui_lbVcore1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbVcore1, "95%");
    lv_obj_set_style_text_color(ui_lbVcore1, lv_color_hex(0xDECE08), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcore1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcore1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcore1, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblBlock = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblBlock, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblBlock, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblBlock, -37);
    lv_obj_set_y(ui_lblBlock, 67);
    lv_obj_set_align(ui_lblBlock, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblBlock, "881.557");
    lv_obj_set_style_text_color(ui_lblBlock, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBlock, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBlock, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBlock, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblBlock1 = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblBlock1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblBlock1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblBlock1, -97);
    lv_obj_set_y(ui_lblBlock1, 68);
    lv_obj_set_align(ui_lblBlock1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblBlock1, "881.557");
    lv_obj_set_style_text_color(ui_lblBlock1, lv_color_hex(0xDECE08), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBlock1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBlock1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBlock1, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblDifficulty = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblDifficulty, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblDifficulty, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblDifficulty, -40);
    lv_obj_set_y(ui_lblDifficulty, -11);
    lv_obj_set_align(ui_lblDifficulty, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblDifficulty, "81T");
    lv_obj_set_style_text_color(ui_lblDifficulty, lv_color_hex(0xC6C6C5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblDifficulty, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblDifficulty, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblDifficulty, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblGlobalHash = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblGlobalHash, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblGlobalHash, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblGlobalHash, -39);
    lv_obj_set_y(ui_lblGlobalHash, 30);
    lv_obj_set_align(ui_lblGlobalHash, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblGlobalHash, "751,45");
    lv_obj_set_style_text_color(ui_lblGlobalHash, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblGlobalHash, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblGlobalHash, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblGlobalHash, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblminFee = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblminFee, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblminFee, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblminFee, 47);
    lv_obj_set_y(ui_lblminFee, -64);
    lv_obj_set_align(ui_lblminFee, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblminFee, "2");
    lv_obj_set_style_text_color(ui_lblminFee, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblminFee, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblminFee, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblminFee, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblmedFee = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblmedFee, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblmedFee, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblmedFee, 89);
    lv_obj_set_y(ui_lblmedFee, -64);
    lv_obj_set_align(ui_lblmedFee, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblmedFee, "200");
    lv_obj_set_style_text_color(ui_lblmedFee, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblmedFee, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblmedFee, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblmedFee, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblmaxFee = lv_label_create(ui_GlobalStats);
    lv_obj_set_width(ui_lblmaxFee, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblmaxFee, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblmaxFee, 138);
    lv_obj_set_y(ui_lblmaxFee, -64);
    lv_obj_set_align(ui_lblmaxFee, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblmaxFee, "1000");
    lv_obj_set_style_text_color(ui_lblmaxFee, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblmaxFee, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblmaxFee, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblmaxFee, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_GlobalStats, ui_event_GlobalStats, LV_EVENT_ALL, NULL);

}

void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_Splash2_screen_init();
    ui_MiningScreen_screen_init();
    ui_SettingsScreen_screen_init();
    ui_BTCScreen_screen_init();
    ui_GlobalStats_screen_init();
    lv_disp_load_scr(ui_Splash2);
}
