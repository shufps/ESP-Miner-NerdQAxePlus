#include "global_state.h"
#include "ui_helpers.h"
#include "ui.h"

#include "displayDriver.h"
#include "esp_log.h"
#include "esp_timer.h"


UI::UI() {
    m_last_screen_change_time = 0;
}

///////////////////// FUNCTIONS ////////////////////


///////////////////// SCREENS ////////////////////

void UI::splash1ScreenInit(void)
{
    ui_Splash1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Splash1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_imgSplash1 = lv_img_create(ui_Splash1);
    lv_img_set_src(ui_imgSplash1, m_theme->getInitScreen());
    lv_obj_set_width(ui_imgSplash1, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_imgSplash1, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_imgSplash1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgSplash1, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_imgSplash1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    // lv_obj_add_event_cb(ui_Splash1, ui_event_Splash1, LV_EVENT_ALL, NULL);

    // Liberar memoria de imágenes no utilizadas
    lv_img_cache_invalidate_src(m_theme->getSplashScreen());
}

void UI::splash2ScreenInit(void)
{
    ui_Splash2 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Splash2, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_Image1 = lv_img_create(ui_Splash2);
    lv_img_set_src(ui_Image1, m_theme->getSplashScreen());
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_lbConnect = lv_label_create(ui_Splash2);
    lv_obj_set_width(ui_lbConnect, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbConnect, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbConnect, -31);
    lv_obj_set_y(ui_lbConnect, -40);
    lv_obj_set_align(ui_lbConnect, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbConnect, "Connecting...");
    lv_obj_set_style_text_color(ui_lbConnect, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbConnect, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbConnect, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbConnect, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    // lv_obj_add_event_cb(ui_Splash2, ui_event_Splash2, LV_EVENT_ALL, NULL);

    // Liberar memoria de imágenes no utilizadas
    lv_img_cache_invalidate_src(m_theme->getInitScreen());
}

void UI::portalScreenInit(void)
{
    ui_PortalScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_PortalScreen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_Image1 = lv_img_create(ui_PortalScreen);
    lv_img_set_src(ui_Image1, m_theme->getPortalScreen());
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_lbSSID = lv_label_create(ui_PortalScreen);
    lv_obj_set_width(ui_lbSSID, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbSSID, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbSSID, 75);
    lv_obj_set_y(ui_lbSSID, 52);
    lv_obj_set_align(ui_lbSSID, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbSSID, "NERDAXE_XXXX");
    lv_obj_set_style_text_color(ui_lbSSID, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbSSID, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbSSID, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbSSID, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    // lv_obj_add_event_cb(ui_Splash2, ui_event_Splash2, LV_EVENT_ALL, NULL);

    // Liberar memoria de imágenes no utilizadas
    lv_img_cache_invalidate_src(m_theme->getInitScreen());
}

void UI::miningScreenInit(void)
{
    ui_MiningScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MiningScreen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_Image2 = lv_img_create(ui_MiningScreen);
    lv_img_set_src(ui_Image2, m_theme->getMiningScreen());
    lv_obj_set_width(ui_Image2, LV_SIZE_CONTENT);  /// 320
    lv_obj_set_height(ui_Image2, LV_SIZE_CONTENT); /// 170
    lv_obj_set_align(ui_Image2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_lbVinput = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbVinput, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbVinput, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbVinput, 234);
    lv_obj_set_y(ui_lbVinput, -34);
    lv_obj_set_align(ui_lbVinput, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVinput, "5V");
    lv_obj_set_style_text_color(ui_lbVinput, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVinput, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVinput, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVinput, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbVcore = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbVcore, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbVcore, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbVcore, 234);
    lv_obj_set_y(ui_lbVcore, -12);
    lv_obj_set_align(ui_lbVcore, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVcore, "1200mV");
    lv_obj_set_style_text_color(ui_lbVcore, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcore, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcore, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcore, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbIntensidad = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbIntensidad, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbIntensidad, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbIntensidad, 234);
    lv_obj_set_y(ui_lbIntensidad, 10);
    lv_obj_set_align(ui_lbIntensidad, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbIntensidad, "2.344mA");
    lv_obj_set_style_text_color(ui_lbIntensidad, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIntensidad, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIntensidad, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIntensidad, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPower = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbPower, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbPower, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbPower, 234);
    lv_obj_set_y(ui_lbPower, 32);
    lv_obj_set_align(ui_lbPower, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPower, "0W");
    lv_obj_set_style_text_color(ui_lbPower, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPower, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPower, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPower, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbEficiency = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbEficiency, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbEficiency, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbEficiency, -43);
    lv_obj_set_y(ui_lbEficiency, 61);
    lv_obj_set_align(ui_lbEficiency, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbEficiency, "12.4");
    lv_obj_set_style_text_color(ui_lbEficiency, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbEficiency, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbEficiency, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbEficiency, &ui_font_DigitalNumbers16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbTemp = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbTemp, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbTemp, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbTemp, -139);
    lv_obj_set_y(ui_lbTemp, 24);
    lv_obj_set_align(ui_lbTemp, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbTemp, "48");
    lv_obj_set_style_text_color(ui_lbTemp, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbTemp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbTemp, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbTemp, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbTime = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbTime, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbTime, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbTime, -190);
    lv_obj_set_y(ui_lbTime, 0);
    lv_obj_set_align(ui_lbTime, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbTime, "1d 2h 5m");
    lv_obj_set_style_text_color(ui_lbTime, lv_color_hex(0xDEEE00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbTime, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbTime, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbTime, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbIP = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbIP, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbIP, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbIP, -16);
    lv_obj_set_y(ui_lbIP, -77);
    lv_obj_set_align(ui_lbIP, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbIP, "192.168.1.200");
    lv_obj_set_style_text_color(ui_lbIP, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIP, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIP, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIP, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbBestDifficulty = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbBestDifficulty, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbBestDifficulty, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbBestDifficulty, 34);
    lv_obj_set_y(ui_lbBestDifficulty, 21);
    lv_obj_set_align(ui_lbBestDifficulty, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbBestDifficulty, "22M");
    lv_obj_set_style_text_color(ui_lbBestDifficulty, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbBestDifficulty, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbBestDifficulty, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbBestDifficulty, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbHashrate = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbHashrate, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbHashrate, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbHashrate, -208);
    lv_obj_set_y(ui_lbHashrate, 59);
    lv_obj_set_align(ui_lbHashrate, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbHashrate, "500,0");
    lv_obj_set_style_text_color(ui_lbHashrate, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbHashrate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbHashrate, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbHashrate, &ui_font_DigitalNumbers28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbRPM = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbRPM, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbRPM, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbRPM, 20);
    lv_obj_set_y(ui_lbRPM, -9);
    lv_obj_set_align(ui_lbRPM, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbRPM, "5000");
    lv_obj_set_style_text_color(ui_lbRPM, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbRPM, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbRPM, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbRPM, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbASIC = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbASIC, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbASIC, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbASIC, 111);
    lv_obj_set_y(ui_lbASIC, -66);
    lv_obj_set_align(ui_lbASIC, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbASIC, m_board->get_asic_model());
    lv_obj_set_style_text_color(ui_lbASIC, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbASIC, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbASIC, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbASIC, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    // lv_obj_add_event_cb(ui_MiningScreen, ui_event_MiningScreen, LV_EVENT_ALL, NULL);
}
void UI::settingsScreenInit(void)
{
    ui_SettingsScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SettingsScreen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_Image4 = lv_img_create(ui_SettingsScreen);
    lv_img_set_src(ui_Image4, m_theme->getSettingsScreen());
    lv_obj_set_width(ui_Image4, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_Image4, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_Image4, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image4, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_Image4, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_lbIPSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbIPSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbIPSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbIPSet, -16);
    lv_obj_set_y(ui_lbIPSet, -77);
    lv_obj_set_align(ui_lbIPSet, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbIPSet, "192.168.1.200");
    lv_obj_set_style_text_color(ui_lbIPSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbIPSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbIPSet, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbIPSet, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbBestDifficultySet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbBestDifficultySet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbBestDifficultySet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbBestDifficultySet, 34);
    lv_obj_set_y(ui_lbBestDifficultySet, 21);
    lv_obj_set_align(ui_lbBestDifficultySet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbBestDifficultySet, "22M");
    lv_obj_set_style_text_color(ui_lbBestDifficultySet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbBestDifficultySet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbBestDifficultySet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbBestDifficultySet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbVcoreSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbVcoreSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbVcoreSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbVcoreSet, 43);
    lv_obj_set_y(ui_lbVcoreSet, -45);
    lv_obj_set_align(ui_lbVcoreSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVcoreSet, "1200mV");
    lv_obj_set_style_text_color(ui_lbVcoreSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcoreSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcoreSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcoreSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbFreqSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbFreqSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbFreqSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbFreqSet, 43);
    lv_obj_set_y(ui_lbFreqSet, -25);
    lv_obj_set_align(ui_lbFreqSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbFreqSet, "485");
    lv_obj_set_style_text_color(ui_lbFreqSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbFreqSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbFreqSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbFreqSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbFanSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbFanSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbFanSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbFanSet, 43);
    lv_obj_set_y(ui_lbFanSet, -5);
    lv_obj_set_align(ui_lbFanSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbFanSet, "AUTO");
    lv_obj_set_style_text_color(ui_lbFanSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbFanSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbFanSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbFanSet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPoolSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbPoolSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbPoolSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbPoolSet, 169);
    lv_obj_set_y(ui_lbPoolSet, -9);
    lv_obj_set_align(ui_lbPoolSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPoolSet, "public-pool.io");
    lv_obj_set_style_text_color(ui_lbPoolSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPoolSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPoolSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPoolSet, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbHashrateSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbHashrateSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbHashrateSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbHashrateSet, -208);
    lv_obj_set_y(ui_lbHashrateSet, 59);
    lv_obj_set_align(ui_lbHashrateSet, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbHashrateSet, "500,0");
    lv_obj_set_style_text_color(ui_lbHashrateSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbHashrateSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbHashrateSet, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbHashrateSet, &ui_font_DigitalNumbers28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbShares = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbShares, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbShares, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbShares, -23);
    lv_obj_set_y(ui_lbShares, 58);
    lv_obj_set_align(ui_lbShares, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lbShares, "0/0");
    lv_obj_set_style_text_color(ui_lbShares, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbShares, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbShares, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbShares, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbPortSet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbPortSet, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lbPortSet, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lbPortSet, 211);
    lv_obj_set_y(ui_lbPortSet, 13);
    lv_obj_set_align(ui_lbPortSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPortSet, "3333");
    lv_obj_set_style_text_color(ui_lbPortSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPortSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPortSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPortSet, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void UI::logScreenInit(void)
{
    ui_LogScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_LogScreen, LV_OBJ_FLAG_SCROLLABLE);

    // Create a black background
    lv_obj_set_style_bg_color(ui_LogScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LogScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Create a label for the log text
    ui_LogLabel = lv_label_create(ui_LogScreen);
    lv_label_set_text(ui_LogLabel, "");
    lv_obj_set_style_text_color(ui_LogLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LogLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LogLabel, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(ui_LogLabel, lv_pct(100)); // Set label width to 100% of the parent
    lv_obj_set_style_text_align(ui_LogLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ui_LogLabel, LV_ALIGN_TOP_LEFT, 0, 0);
}

void UI::bTCScreenInit(void)
{
    ui_BTCScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_BTCScreen, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_BTCScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BTCScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ImgBTCscreen = lv_img_create(ui_BTCScreen);
    lv_img_set_src(ui_ImgBTCscreen, m_theme->getBtcScreen());
    lv_obj_set_width(ui_ImgBTCscreen, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_ImgBTCscreen, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_ImgBTCscreen, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImgBTCscreen, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_ImgBTCscreen, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_lblBTCPrice = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblBTCPrice, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lblBTCPrice, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lblBTCPrice, 30);
    lv_obj_set_y(ui_lblBTCPrice, 47);
    lv_obj_set_align(ui_lblBTCPrice, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblBTCPrice, "0$");
    lv_obj_set_style_text_color(ui_lblBTCPrice, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBTCPrice, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBTCPrice, &ui_font_OpenSansBold45, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*ui_lblPriceInc = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblPriceInc, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lblPriceInc, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lblPriceInc, 193);
    lv_obj_set_y(ui_lblPriceInc, 49);
    lv_obj_set_align(ui_lblPriceInc, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblPriceInc, "2%");
    lv_obj_set_style_text_color(ui_lblPriceInc, lv_color_hex(0x07FF2A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblPriceInc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblPriceInc, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);*/

    ui_lblHashPrice = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblHashPrice, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lblHashPrice, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lblHashPrice, 236);
    lv_obj_set_y(ui_lblHashPrice, -63);
    lv_obj_set_align(ui_lblHashPrice, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblHashPrice, "500,0");
    lv_obj_set_style_text_color(ui_lblHashPrice, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHashPrice, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHashPrice, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHashPrice, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblTempPrice = lv_label_create(ui_BTCScreen);
    lv_obj_set_width(ui_lblTempPrice, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_lblTempPrice, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_lblTempPrice, 261);
    lv_obj_set_y(ui_lblTempPrice, -18);
    lv_obj_set_align(ui_lblTempPrice, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblTempPrice, "24");
    lv_obj_set_style_text_color(ui_lblTempPrice, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblTempPrice, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblTempPrice, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblTempPrice, &ui_font_OpenSansBold24, LV_PART_MAIN | LV_STATE_DEFAULT);
}


// Function to show the overlay with the overheating image
void UI::showOverheatWarningOverlay()
{
    // Get the currently active screen
    lv_obj_t *current_screen = lv_scr_act();

    // Create a container for the overlay
    lv_obj_t *overlay_container = lv_obj_create(current_screen);
    lv_obj_set_size(overlay_container, 274 + 4, 65 + 4); // Set the size of the overlay box
    lv_obj_align(overlay_container, LV_ALIGN_CENTER, 0, -20); // Center the overlay on the screen

    // Disable scrollbars for the container
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_SCROLLABLE);

    // Set background color and border style if needed
    lv_obj_set_style_bg_color(overlay_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay_container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(overlay_container, lv_color_hex(0xff0000), LV_PART_MAIN);

    // Create the image inside the overlay container
    lv_obj_t *overlay_image = lv_img_create(overlay_container);
    lv_img_set_src(overlay_image, &ui_img_overheat_png); // Use the existing overheating image
    lv_obj_set_width(overlay_image, LV_SIZE_CONTENT);  // Adjust width based on content
    lv_obj_set_height(overlay_image, LV_SIZE_CONTENT); // Adjust height based on content
    lv_obj_set_align(overlay_image, LV_ALIGN_CENTER);  // Center the image in the overlay

    // Add any additional flags or styles as needed
    lv_obj_add_flag(overlay_image, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(overlay_image, LV_OBJ_FLAG_SCROLLABLE);
}

void UI::hideOverheatWarningOverlay(lv_obj_t *overlay_container)
{
    if (overlay_container != NULL) {
        lv_obj_del(overlay_container);
        overlay_container = NULL; // Clear the pointer
    }
}

void UI::init(Board* m_board)
{
    this->m_board = m_board;
    this->m_theme = m_board->getTheme();

    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *m_theme =
        lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, m_theme);
    if (ui_Splash1 == NULL)
        splash1ScreenInit();
    // ui_Splash2_screen_init();
    // ui_Portal_screen_init();
    // ui_MiningScreen_screen_init();
    // ui_SettingsScreen_screen_init();
    // ui_LogScreen_init();
    lv_disp_load_scr(ui_Splash1);
}
