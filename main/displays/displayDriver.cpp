#include <inttypes.h>
#include <stdio.h>

#include "APIs.h"
#include "lv_conf.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "ui_helpers.h"
#include "global_state.h"
#include "system.h"

#include "nvs_config.h"
#include "displayDriver.h"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char *TAG = "TDisplayS3";

DisplayDriver::DisplayDriver() {
    m_animationsEnabled = false;
    m_button1PressedFlag = false;
    m_button2PressedFlag = false;
    m_lastKeypressTime = 0;
    m_displayIsOn = true;
    m_screenStatus = STATE_ONINIT;
    m_nextScreen = 0;
    m_countdownActive = false;
    m_countdownStartTime = 0;
    m_btcPrice = 0;
}

bool DisplayDriver::notifyLvglFlushReady(esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* edata,
                                   void* userCtx) {
    lv_disp_drv_t* dispDriver = (lv_disp_drv_t*)userCtx;
    lv_disp_flush_ready(dispDriver);
    return false;
}

void DisplayDriver::lvglFlushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colorMap) {
    esp_lcd_panel_handle_t panelHandle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // Copy buffer content to the display
    esp_lcd_panel_draw_bitmap(panelHandle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, colorMap);
}

/************ DISPLAY TURN ON/OFF FUNCTIONS *************/
void DisplayDriver::displayTurnOff(void) {
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);
    gpio_set_level(TDISPLAYS3_PIN_PWR, false);
    ESP_LOGI(TAG, "Screen off");
    m_displayIsOn = false;
}

void DisplayDriver::displayTurnOn(void) {
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Screen on");
    m_displayIsOn = true;
}

/************ AUTO TURN OFF DISPLAY FUNCTIONS *************/
void DisplayDriver::startCountdown(void) {
    m_countdownActive = true;
    m_countdownStartTime = esp_timer_get_time();

    if (m_countdownLabel == NULL) {
        lv_obj_t* currentScreen = lv_scr_act();
        lv_obj_t* blackBox = lv_obj_create(currentScreen);
        lv_obj_set_size(blackBox, 200, 100);
        lv_obj_set_style_bg_color(blackBox, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(blackBox, 0, LV_PART_MAIN);
        lv_obj_align(blackBox, LV_ALIGN_CENTER, 0, 0);

        m_countdownLabel = lv_label_create(blackBox);
        lv_label_set_text(m_countdownLabel, "Turning screen off...");
        lv_obj_set_style_text_color(m_countdownLabel, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(m_countdownLabel);
    }
}

void DisplayDriver::displayHideCountdown(void) {
    if (m_countdownLabel) {
        lv_obj_del(lv_obj_get_parent(m_countdownLabel));
        m_countdownLabel = NULL;
    }
}

void DisplayDriver::checkAutoTurnOffScreen(void) {
    if (!m_displayIsOn)
        return;

    int64_t currentTime = esp_timer_get_time();

    if ((currentTime - m_lastKeypressTime) > 30000000) {  // 30 seconds timeout
        if (!m_countdownActive) {
            startCountdown();
        }

        int64_t elapsedTime = (currentTime - m_countdownStartTime) / 1000000;  // Convert to seconds

        if (elapsedTime > 5) {
            displayHideCountdown();
            displayTurnOff();
            m_countdownActive = false;
        }

    } else {
        if (m_countdownActive) {
            displayHideCountdown();
            m_countdownActive = false;
        }
    }
}

void DisplayDriver::increaseLvglTick() {
    lv_tick_inc(TDISPLAYS3_LVGL_TICK_PERIOD_MS);
}

// Refresh screen values
void DisplayDriver::refreshScreen(void) {
    lv_timer_handler();
    increaseLvglTick();
}

void DisplayDriver::showError(const char *error_message, uint32_t error_code) {
    m_ui->showErrorOverlay(error_message, error_code);
    refreshScreen();
}

void DisplayDriver::changeScreen(void) {
    if (m_screenStatus == SCREEN_MINING) {
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        m_screenStatus = SCREEN_SETTINGS;
        ESP_LOGI("UI", "New Screen Settings displayed");
    } else if (m_screenStatus == SCREEN_SETTINGS) {
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_BTCScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        m_screenStatus = SCREEN_BTCPRICE;
        ESP_LOGI("UI", "New Screen BTCprice displayed");
    } else if (m_screenStatus == SCREEN_BTCPRICE) {
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 350, 0);
        m_screenStatus = SCREEN_MINING;
        ESP_LOGI("UI", "New Screen Mining displayed");
    }
}

void DisplayDriver::lvglTimerTaskWrapper(void *param) {
    DisplayDriver *display = (DisplayDriver*) param;
    display->lvglTimerTask(NULL);
}

void DisplayDriver::lvglTimerTask(void *param)
{
    int64_t myLastTime = esp_timer_get_time();
    uint8_t autoOffEnabled = nvs_config_get_u16(NVS_CONFIG_AUTO_SCREEN_OFF, 0);
    // int64_t current_time = esp_timer_get_time();

    // Check if screen is changing to avoid problems during change
    // if ((current_time - last_screen_change_time) < 1500000) return; // 1500000 microsegundos = 1500 ms = 1.5s - No cambies
    // pantalla last_screen_change_time = current_time;

    int32_t elapsed_Ani_cycles = 0;
    while (1) {

        // Enabled when change screen animation is activated
        if (m_animationsEnabled) {
            increaseLvglTick();
            lv_timer_handler();                 // Process pending LVGL tasks
            vTaskDelay(5 / portTICK_PERIOD_MS); // Delay during animations
            if (elapsed_Ani_cycles++ > 80) {
                // After 1s aprox stop animations
                m_animationsEnabled = false;
                elapsed_Ani_cycles = 0;
            }
        } else {
            if (m_button1PressedFlag) {
                m_button1PressedFlag = false;
                m_lastKeypressTime = esp_timer_get_time();
                if (!m_displayIsOn)
                    displayTurnOn();
                changeScreen();
            }
            vTaskDelay(200 / portTICK_PERIOD_MS); // Delay waiting animation trigger
        }
        if (m_button2PressedFlag) {
            m_button2PressedFlag = false;
            m_lastKeypressTime = esp_timer_get_time();
            if (m_displayIsOn)
                displayTurnOff();
            else
                displayTurnOn();
        }

        // Check if screen need to be turned off
        if (autoOffEnabled)
            checkAutoTurnOffScreen();

        if ((m_screenStatus > STATE_INIT_OK))
            continue; // Doesn't need to do the initial animation screens

        // Screen initial process
        int32_t elapsed = (esp_timer_get_time() - myLastTime) / 1000;
        switch (m_screenStatus) {
        case STATE_ONINIT: // First splash Screen
            if (elapsed > 3000) {
                ESP_LOGI(TAG, "Changing Screen to SPLASH2");
                if (m_ui->ui_Splash2 == NULL)
                    m_ui->splash2ScreenInit();
                enableLvglAnimations(true);
                _ui_screen_change(m_ui->ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                m_screenStatus = STATE_SPLASH1;
                myLastTime = esp_timer_get_time();
            }
            break;
        case STATE_SPLASH1: // Second splash screen
            if (elapsed > 3000) {
                // Init done, wait until on portal or mining is shown
                m_screenStatus = STATE_INIT_OK;
                ESP_LOGI(TAG, "Changing Screen to WAIT SELECTION");
                if (m_ui->ui_Splash1) {
                    lv_obj_clean(m_ui->ui_Splash1);
                }
                m_ui->ui_Splash1 = NULL;
            }
            break;
        case STATE_INIT_OK: // Show portal
            if (m_nextScreen == SCREEN_PORTAL) {
                ESP_LOGI(TAG, "Changing Screen to Show Portal");
                m_screenStatus = SCREEN_PORTAL;
                if (m_ui->ui_PortalScreen == NULL) {
                    m_ui->portalScreenInit();
                }
                lv_label_set_text(m_ui->ui_lbSSID, m_portalWifiName); // Actualiza el label
                enableLvglAnimations(true);
                _ui_screen_change(m_ui->ui_PortalScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                if (m_ui->ui_Splash2) {
                    lv_obj_clean(m_ui->ui_Splash2);
                }
                m_ui->ui_Splash2 = NULL;
            } else if (m_nextScreen == SCREEN_MINING) {
                // Show Mining screen
                ESP_LOGI(TAG, "Changing Screen to Mining screen");
                m_screenStatus = SCREEN_MINING;
                if (m_ui->ui_MiningScreen == NULL)
                    m_ui->miningScreenInit();
                if (m_ui->ui_SettingsScreen == NULL)
                    m_ui->settingsScreenInit();
                if (m_ui->ui_BTCScreen == NULL)
                    m_ui->bTCScreenInit();
                enableLvglAnimations(true);
                _ui_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                if (m_ui->ui_Splash2) {
                    lv_obj_clean(m_ui->ui_Splash2);
                }
                m_ui->ui_Splash2 = NULL;
            }
            break;
        }
    }
}

// Función para activar las actualizaciones
void DisplayDriver::enableLvglAnimations(bool enable)
{
    m_animationsEnabled = enable;
}

void DisplayDriver::mainCreatSysteTasks(void)
{
    xTaskCreatePinnedToCore(lvglTimerTaskWrapper, "lvgl Timer", 6000, (void*) this, 4, NULL, 1); // Antes 10000
}

lv_obj_t *DisplayDriver::initTDisplayS3(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    // GPIO configuration
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {.pin_bit_mask = 1ULL << TDISPLAYS3_PIN_NUM_BK_LIGHT, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
    gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
    gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);

    gpio_set_direction(TDISPLAYS3_PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT);
    gpio_set_direction(TDISPLAYS3_PIN_RD, GPIO_MODE_OUTPUT);
    gpio_set_direction(TDISPLAYS3_PIN_PWR, GPIO_MODE_OUTPUT);

    gpio_set_level(TDISPLAYS3_PIN_RD, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {.dc_gpio_num = TDISPLAYS3_PIN_NUM_DC,
                                           .wr_gpio_num = TDISPLAYS3_PIN_NUM_PCLK,
                                           .clk_src = LCD_CLK_SRC_DEFAULT,
                                           .data_gpio_nums =
                                               {
                                                   TDISPLAYS3_PIN_NUM_DATA0,
                                                   TDISPLAYS3_PIN_NUM_DATA1,
                                                   TDISPLAYS3_PIN_NUM_DATA2,
                                                   TDISPLAYS3_PIN_NUM_DATA3,
                                                   TDISPLAYS3_PIN_NUM_DATA4,
                                                   TDISPLAYS3_PIN_NUM_DATA5,
                                                   TDISPLAYS3_PIN_NUM_DATA6,
                                                   TDISPLAYS3_PIN_NUM_DATA7,
                                               },
                                           .bus_width = 8,
                                           .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
                                           .psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
                                           .sram_trans_align = LCD_SRAM_TRANS_ALIGN};
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = TDISPLAYS3_PIN_NUM_CS,
        .pclk_hz = TDISPLAYS3_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .on_color_trans_done = notifyLvglFlushReady,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = TDISPLAYS3_LCD_CMD_BITS,
        .lcd_param_bits = TDISPLAYS3_LCD_PARAM_BITS,
        .dc_levels =
            {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            }
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install LCD driver of st7789");
    esp_lcd_panel_handle_t panel_handle = NULL;

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TDISPLAYS3_PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);

    esp_lcd_panel_swap_xy(panel_handle, true);

    if (!nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, 0)) {
        esp_lcd_panel_mirror(panel_handle, true, false);
    } else {
        esp_lcd_panel_mirror(panel_handle, false, true);
    }

    // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
    esp_lcd_panel_set_gap(panel_handle, 0, 35);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t*) heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    //    lv_color_t *buf2 = heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA );
    //    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LVGL_LCD_BUF_SIZE);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TDISPLAYS3_LCD_H_RES;
    disp_drv.ver_res = TDISPLAYS3_LCD_V_RES;
    disp_drv.flush_cb = lvglFlushCallback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // Configuration is completed.

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    /*const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increaseLvglTick,
        .name = "lvgl_tick"
    };*/
    esp_timer_handle_t lvgl_tick_timer = NULL;
    // ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    // ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, TDISPLAYS3_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL animation");
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    return scr;
}

void DisplayDriver::updateHashrate(System *module, float power)
{
    char strData[20];

    float efficiency = power / (module->getCurrentHashrate10m() / 1000.0);
    float hashrate = module->getCurrentHashrate10m();

    // >= 10T doesn't fit on the screen with a decimal place
    if (hashrate >= 10000.0) {
        snprintf(strData, sizeof(strData), "%d", (int) (hashrate + 0.5f));
    } else {
        snprintf(strData, sizeof(strData), "%.1f", hashrate);
    }

    lv_label_set_text(m_ui->ui_lbHashrate, strData);    // Update hashrate
    lv_label_set_text(m_ui->ui_lbHashrateSet, strData); // Update hashrate
    lv_label_set_text(m_ui->ui_lblHashPrice, strData);  // Update hashrate

    snprintf(strData, sizeof(strData), "%.1f", efficiency);
    lv_label_set_text(m_ui->ui_lbEficiency, strData); // Update eficiency label

    snprintf(strData, sizeof(strData), "%.3fW", power);
    lv_label_set_text(m_ui->ui_lbPower, strData); // Actualiza el label
}

void DisplayDriver::updateShares(System *module)
{
    char strData[20];

    snprintf(strData, sizeof(strData), "%lld/%lld", module->getSharesAccepted(), module->getSharesRejected());
    lv_label_set_text(m_ui->ui_lbShares, strData); // Update shares

    snprintf(strData, sizeof(strData), "%s", module->getBestDiffString());
    lv_label_set_text(m_ui->ui_lbBestDifficulty, module->getBestDiffString());    // Update Bestdifficulty
    lv_label_set_text(m_ui->ui_lbBestDifficultySet, module->getBestDiffString()); // Update Bestdifficulty
}
void DisplayDriver::updateTime(System *module)
{
    char strData[20];

    // Calculate the uptime in seconds
    // int64_t currentTimeTest = esp_timer_get_time() + (8 * 3600 * 1000000LL) + (1800 * 1000000LL);//(8 * 60 * 60 * 10000);
    double uptime_in_seconds = (esp_timer_get_time() - module->getStartTime()) / 1000000;
    int uptime_in_days = uptime_in_seconds / (3600 * 24);
    int remaining_seconds = (int) uptime_in_seconds % (3600 * 24);
    int uptime_in_hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    int uptime_in_minutes = remaining_seconds / 60;
    int current_seconds = remaining_seconds % 60;

    snprintf(strData, sizeof(strData), "%dd %ih %im %is", uptime_in_days, uptime_in_hours, uptime_in_minutes, current_seconds);
    lv_label_set_text(m_ui->ui_lbTime, strData); // Update label
}

void DisplayDriver::updateCurrentSettings()
{
    char strData[20];
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    lv_label_set_text(m_ui->ui_lbPoolSet, SYSTEM_MODULE.getPoolUrl()); // Update label

    snprintf(strData, sizeof(strData), "%d", SYSTEM_MODULE.getPoolPort());
    lv_label_set_text(m_ui->ui_lbPortSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY));
    lv_label_set_text(m_ui->ui_lbFreqSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE));
    lv_label_set_text(m_ui->ui_lbVcoreSet, strData); // Update label

    uint16_t auto_fan_speed = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1);
    if (auto_fan_speed == 1)
        lv_label_set_text(m_ui->ui_lbFanSet, "AUTO"); // Update label
    else {
        snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100));
        lv_label_set_text(m_ui->ui_lbFanSet, strData); // Update label
    }
}


void DisplayDriver::updateBTCprice(void)
{
    char price_str[32];

    if ((m_screenStatus != SCREEN_BTCPRICE) && (m_btcPrice != 0))
        return;

    m_btcPrice = getBTCprice();
    snprintf(price_str, sizeof(price_str), "%u$", m_btcPrice);
    lv_label_set_text(m_ui->ui_lblBTCPrice, price_str); // Update label
}

void DisplayDriver::updateGlobalState()
{
    char strData[20];

    if (m_ui->ui_MiningScreen == NULL)
        return;
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    // snprintf(strData, sizeof(strData), "%.0f", power_management->chip_temp);
    snprintf(strData, sizeof(strData), "%.0f", POWER_MANAGEMENT_MODULE.getAvgChipTemp());
    lv_label_set_text(m_ui->ui_lbTemp, strData);       // Update label
    lv_label_set_text(m_ui->ui_lblTempPrice, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", POWER_MANAGEMENT_MODULE.getFanRPM());
    lv_label_set_text(m_ui->ui_lbRPM, strData); // Update label

    snprintf(strData, sizeof(strData), "%.3fW", POWER_MANAGEMENT_MODULE.getPower());
    lv_label_set_text(m_ui->ui_lbPower, strData); // Update label

    snprintf(strData, sizeof(strData), "%imA", (int) POWER_MANAGEMENT_MODULE.getCurrent());
    lv_label_set_text(m_ui->ui_lbIntensidad, strData); // Update label

    snprintf(strData, sizeof(strData), "%imV", (int) POWER_MANAGEMENT_MODULE.getVoltage());
    lv_label_set_text(m_ui->ui_lbVinput, strData); // Update label

    updateTime(&SYSTEM_MODULE);
    updateShares(&SYSTEM_MODULE);
    updateHashrate(&SYSTEM_MODULE, POWER_MANAGEMENT_MODULE.getPower());
    updateBTCprice();

    Board *board = SYSTEM_MODULE.getBoard();
    uint16_t vcore = (int) (board->getVout() * 1000.0f);
    snprintf(strData, sizeof(strData), "%umV", vcore);
    lv_label_set_text(m_ui->ui_lbVcore, strData); // Update label
}

void DisplayDriver::updateIpAddress(char *ip_address_str)
{
    if (m_ui->ui_MiningScreen == NULL)
        return;
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    lv_label_set_text(m_ui->ui_lbIP, ip_address_str);    // Update label
    lv_label_set_text(m_ui->ui_lbIPSet, ip_address_str); // Update label
}

void DisplayDriver::logMessage(const char *message)
{
    m_screenStatus = SCREEN_LOG;
    if (m_ui->ui_LogScreen == NULL)
        m_ui->logScreenInit();
    lv_label_set_text(m_ui->ui_LogLabel, message);
    enableLvglAnimations(true);
    _ui_screen_change(m_ui->ui_LogScreen, LV_SCR_LOAD_ANIM_NONE, 500, 0);
}

void DisplayDriver::miningScreen(void)
{
    // Only called once at the beggining from system lib
    if (m_ui->ui_MiningScreen == NULL)
        m_ui->miningScreenInit();
    if (m_ui->ui_SettingsScreen == NULL)
        m_ui->settingsScreenInit();
    if (m_ui->ui_BTCScreen == NULL)
        m_ui->bTCScreenInit();
    m_nextScreen = SCREEN_MINING;
}

void DisplayDriver::portalScreen(const char *message)
{
    m_nextScreen = SCREEN_PORTAL;
    strcpy(m_portalWifiName, message);
}
void DisplayDriver::updateWifiStatus(const char *message)
{
    if (m_ui->ui_lbConnect != NULL)
        lv_label_set_text(m_ui->ui_lbConnect, message); // Actualiza el label
    refreshScreen();
}

// ISR Handler para el DownButton (Change Screen)
void DisplayDriver::button1IsrHandler(void *arg)
{
    DisplayDriver *display = (DisplayDriver*) arg;
    // ESP_LOGI("UI", "Button pressed changing screen");
    display->m_button1PressedFlag = true;
}

// ISR Handler para el UpButton (Change Screen)
void DisplayDriver::button2IsrHandler(void *arg)
{
    DisplayDriver *display = (DisplayDriver*) arg;
    display->m_button2PressedFlag = true;
}

void DisplayDriver::buttonsInit(void)
{
    gpio_pad_select_gpio(PIN_BUTTON_1);
    gpio_set_direction(PIN_BUTTON_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_1, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_1, GPIO_INTR_POSEDGE); // Interrupción en flanco de bajada

    gpio_pad_select_gpio(PIN_BUTTON_2);
    gpio_set_direction(PIN_BUTTON_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_2, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_2, GPIO_INTR_POSEDGE); // Interrupción en flanco de bajada

    // Habilita las interrupciones de GPIO
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON_1, button1IsrHandler, (void*) this);
    gpio_isr_handler_add(PIN_BUTTON_2, button2IsrHandler, (void*) this);
}

/**
 * @brief Program starts from here
 *
 */
void DisplayDriver::init(Board* board)
{
    ESP_LOGI("INFO", "Setting Up TDisplayS3 Screen");

    // Inicializa el GPIO para el botón
    buttonsInit();

    lv_obj_t *scr = initTDisplayS3();

    m_ui = new UI();
    m_ui->init(board);
    // manual_lvgl_update();

    // startUpdateScreenTask(); //Start screen update task
    mainCreatSysteTasks();
}
/**************************  Useful Electronics  ****************END OF FILE***/
