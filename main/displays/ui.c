#include "ui.h"
#include "ui_helpers.h"

#include "esp_log.h"
#include "esp_timer.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t *current_screen = NULL;

void ui_event_Splash1(lv_event_t * e);
void ui_event_imgSplash1(lv_event_t * e);
lv_obj_t * ui_Splash1;
lv_obj_t * ui_Splash2;
lv_obj_t * ui_PortalScreen;
lv_obj_t * ui_MiningScreen;
lv_obj_t * ui_SettingsScreen;
lv_obj_t * ui_lbSSID;
lv_obj_t * ui_imgSplash1;
lv_obj_t * ui_Image1;
lv_obj_t * ui_lbConnect;
lv_obj_t * ui_Image2;
lv_obj_t * ui_lbVinput;
lv_obj_t * ui_lbVcore;
lv_obj_t * ui_lbIntensidad;
lv_obj_t * ui_lbPower;
lv_obj_t * ui_lbEficiency;
lv_obj_t * ui_lbTemp;
lv_obj_t * ui_lbTime;
lv_obj_t * ui_lbIP;
lv_obj_t * ui_lbBestDifficulty;
lv_obj_t * ui_lbBestDifficultySet;
lv_obj_t * ui_lbHashrate;
lv_obj_t * ui_lbRPM;
lv_obj_t * ui_lbASIC;
lv_obj_t * ui_Image4;
lv_obj_t * ui_lbIPSet;
lv_obj_t * ui_lbVcoreSet;
lv_obj_t * ui_lbFreqSet;
lv_obj_t * ui_lbFanSet;
lv_obj_t * ui_lbPoolSet;
lv_obj_t * ui_lbHashrateSet;
lv_obj_t * ui_lbShares;
lv_obj_t * ui_lbPortSet;


///////////////////// FUNCTIONS ////////////////////

int64_t last_screen_change_time = 0;

void changeScreen(void * arg) {
    int64_t current_time = esp_timer_get_time();
    
    //Check if screen is changing to avoid problems during change
    if (current_time - last_screen_change_time < 500000) return; // 500000 microsegundos = 500 ms - No cambies pantalla
    last_screen_change_time = current_time;
    
    if(current_screen == ui_MiningScreen) {
        _ui_screen_change(ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 400, 0);
        current_screen = ui_SettingsScreen; // Actualiza la pantalla actual
        ESP_LOGI("UI", "NewScreen Settings displayed");
    } else {
        _ui_screen_change(ui_MiningScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 400, 0);
        current_screen = ui_MiningScreen; // Actualiza la pantalla actual
        ESP_LOGI("UI", "NewScreen Mining displayed");
    }

}

void ui_event_Splash1(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_SCREEN_LOADED) {
        _ui_screen_change(ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 1500);
    }
}

void ui_event_Splash2(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_SCREEN_LOADED) {
        _ui_screen_change(ui_MiningScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 1500);
    }
}

///////////////////// SCREENS ////////////////////
void ui_Splash1_screen_init(void)
{
    ui_Splash1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Splash1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_imgSplash1 = lv_img_create(ui_Splash1);
    lv_img_set_src(ui_imgSplash1, &ui_img_initscreen2_png);
    lv_obj_set_width(ui_imgSplash1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_imgSplash1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_imgSplash1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgSplash1, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_imgSplash1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_add_event_cb(ui_Splash1, ui_event_Splash1, LV_EVENT_ALL, NULL);

}
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

    lv_obj_add_event_cb(ui_Splash2, ui_event_Splash2, LV_EVENT_ALL, NULL);

}

/*void ui_Portal_screen_init(void)
{
    ui_PortalScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_PortalScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image1 = lv_img_create(ui_PortalScreen);
    lv_img_set_src(ui_Image1, &ui_img_splashscreen2_png);
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_lbSSID = lv_label_create(ui_PortalScreen);
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

    lv_obj_add_event_cb(ui_Splash2, ui_event_Splash2, LV_EVENT_ALL, NULL);

}*/
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

    ui_lbVinput = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbVinput, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbVinput, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbVinput, 234);
    lv_obj_set_y(ui_lbVinput, -34);
    lv_obj_set_align(ui_lbVinput, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVinput, "1200mV");
    lv_obj_set_style_text_color(ui_lbVinput, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVinput, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVinput, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVinput, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbVcore = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbVcore, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbVcore, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbVcore, 234);
    lv_obj_set_y(ui_lbVcore, -12);
    lv_obj_set_align(ui_lbVcore, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbVcore, "12,2W");
    lv_obj_set_style_text_color(ui_lbVcore, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbVcore, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbVcore, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbVcore, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

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

    ui_lbPower = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbPower, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbPower, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbPower, 234);
    lv_obj_set_y(ui_lbPower, 32);
    lv_obj_set_align(ui_lbPower, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbPower, "0rpm");
    lv_obj_set_style_text_color(ui_lbPower, lv_color_hex(0xDEDEDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPower, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPower, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPower, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

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

    ui_lbRPM = lv_label_create(ui_MiningScreen);
    lv_obj_set_width(ui_lbRPM, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbRPM, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbRPM, 20);
    lv_obj_set_y(ui_lbRPM, -9);
    lv_obj_set_align(ui_lbRPM, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbRPM, "5000");
    lv_obj_set_style_text_color(ui_lbRPM, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbRPM, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbRPM, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbRPM, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

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

    ui_lbBestDifficultySet = lv_label_create(ui_SettingsScreen);
    lv_obj_set_width(ui_lbBestDifficultySet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_lbBestDifficultySet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_lbBestDifficultySet, 34);
    lv_obj_set_y(ui_lbBestDifficultySet, 21);
    lv_obj_set_align(ui_lbBestDifficultySet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lbBestDifficultySet, "22M");
    lv_obj_set_style_text_color(ui_lbBestDifficultySet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbBestDifficultySet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbBestDifficultySet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbBestDifficultySet, &ui_font_OpenSansBold14, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    lv_obj_set_style_text_color(ui_lbPoolSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_text_color(ui_lbPortSet, lv_color_hex(0xDEDADE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbPortSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbPortSet, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbPortSet, &ui_font_OpenSansBold13, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbBestDifficulty = lv_label_create(ui_SettingsScreen);
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

}

void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_Splash1_screen_init();
    ui_Splash2_screen_init();
    ui_MiningScreen_screen_init();
    ui_SettingsScreen_screen_init();
    lv_disp_load_scr(ui_Splash1);

    current_screen = ui_MiningScreen;
}
