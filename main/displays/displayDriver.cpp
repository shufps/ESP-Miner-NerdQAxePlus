#include <inttypes.h>
#include <stdio.h>

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
#include "ui_ipc.h"
#include "ui_helpers.h"
#include "global_state.h"
#include "system.h"
#include "macros.h"
#include "button.h"

#include "nvs_config.h"
#include "displayDriver.h"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char *TAG = "TDisplayS3";

#ifdef NERDQX
#define SPLASH1_TIMEOUT_MS 3000
#define SPLASH2_TIMEOUT_MS 5000
#else
#define SPLASH1_TIMEOUT_MS 3000
#define SPLASH2_TIMEOUT_MS 3000
#endif

// small helpers
static inline int64_t now_us() { return esp_timer_get_time(); }
static inline int32_t elapsed_ms(int64_t start_us, int64_t now) {
    return static_cast<int32_t>((now - start_us) / 1000);
}

DisplayDriver::DisplayDriver() {
    m_animationsEnabled = false;
    m_lastKeypressTime = 0;
    m_displayIsOn = false;
    m_countdownActive = false;
    m_countdownStartTime = 0;
    m_btcPrice = 0;
    m_blockHeight = 0;
    m_isActiveOverlay = false;
    m_lvglMutex = PTHREAD_MUTEX_INITIALIZER;
    m_isAutoScreenOffEnabled = false;
    m_tempControlMode = 0;
    m_fanSpeed = 0;
}

void DisplayDriver::loadSettings() {
    PThreadGuard lock(m_lvglMutex);
    m_isAutoScreenOffEnabled = Config::isAutoScreenOffEnabled();
    m_tempControlMode = Config::getTempControlMode();
    m_fanSpeed = Config::getFanSpeed();

    // when setting was changed, turn on the display LED
    if (!m_isAutoScreenOffEnabled) {
        displayTurnOn();
    }
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
bool DisplayDriver::displayTurnOff(void) {
    if (!m_displayIsOn) {
        return false;
    }
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);
    gpio_set_level(TDISPLAYS3_PIN_PWR, false);
    ESP_LOGI(TAG, "Screen off");
    m_displayIsOn = false;
    return true;
}

bool DisplayDriver::displayTurnOn(void) {
    if (m_displayIsOn) {
        return false;
    }
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Screen on");
    m_displayIsOn = true;
    return true;
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
    // NOP
}

void DisplayDriver::showError(const char *error_message, uint32_t error_code) {
    PThreadGuard lock(m_lvglMutex);
    // hide the overlay and free the memory in case it was open
    m_ui->hideErrorOverlay();

    // now show the (new) error overlay
    m_ui->showErrorOverlay(error_message, error_code);
    m_isActiveOverlay = true;
    refreshScreen();
}

void DisplayDriver::hideError() {
    PThreadGuard lock(m_lvglMutex);
    // hide the overlay and free the memory
    m_ui->hideErrorOverlay();
    m_isActiveOverlay = false;
}

void DisplayDriver::showFoundBlockOverlay() {
    PThreadGuard lock(m_lvglMutex);
    // hide the overlay and free the memory in case it was open
    m_ui->hideImageOverlay();

    // now show the (new) image overlay
    m_ui->showImageOverlay(&ui_img_found_block_png);
    m_isActiveOverlay = true;
    refreshScreen();
}

void DisplayDriver::hideFoundBlockOverlay() {
    // hide the overlay and free the memory
    m_ui->hideImageOverlay();
    m_isActiveOverlay = false;
}

void DisplayDriver::lvglTimerTaskWrapper(void *param) {
    DisplayDriver *display = (DisplayDriver*) param;
    display->lvglTimerTask(NULL);
}


bool DisplayDriver::enterState(UiState s, int64_t now)
{
    // we already are in this state
    if (m_state == s) {
        return true;
    }
    UiState previousState = m_state;

    m_state = s;
    m_stateStart_us = now;

    switch (m_state) {
    case UiState::NOP:
        // NOP
        break;
    case UiState::Splash1:
        ESP_LOGI(TAG, "enter state splash1");
        enableLvglAnimations(true);
        break;

    case UiState::Splash2:
        ESP_LOGI(TAG, "enter state splash2");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        if (m_ui->ui_Splash1) { lv_obj_clean(m_ui->ui_Splash1); m_ui->ui_Splash1 = NULL; }
        break;

    case UiState::Wait:
        ESP_LOGI(TAG, "enter state wait");
        if (m_ui->ui_Splash2) { lv_obj_clean(m_ui->ui_Splash2); m_ui->ui_Splash2 = NULL; }
        break;

    case UiState::Portal:
        ESP_LOGI(TAG, "enter state portal");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_PortalScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        break;

    case UiState::Mining:
        ESP_LOGI(TAG, "enter state mining");
        enableLvglAnimations(true);
        if (previousState == UiState::GlobalStats) {
            _ui_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        } else {
            _ui_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        }
        break;

    case UiState::SettingsScreen:
        ESP_LOGI(TAG, "enter state settings screen");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;

    case UiState::BTCScreen:
        ESP_LOGI(TAG, "enter state btc screen");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_BTCScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;

    case UiState::GlobalStats:
        ESP_LOGI(TAG, "enter state global stats");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_GlobalStats, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;
    case UiState::ShowQR:
        ESP_LOGI(TAG, "enter qr state");
        enableLvglAnimations(true);
        _ui_screen_change(m_ui->ui_qrScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        break;
    }
    return true;
}


void DisplayDriver::updateState(int64_t now, bool btn1Press, bool btn2Press, bool btnBothLongPress)
{
    const int ms = elapsed_ms(m_stateStart_us, now);

    switch (m_state) {
    case UiState::NOP:
        // NOP
        break;
    case UiState::Splash1:
        if (ms >= SPLASH1_TIMEOUT_MS) {
            enterState(UiState::Splash2, now);
        }
        break;

    case UiState::Splash2:
        if (ms >= SPLASH2_TIMEOUT_MS) {
            enterState(UiState::Wait, now);
        }
        break;

    case UiState::Wait:
        // NOP
        break;

    case UiState::Portal:
        enterState(UiState::Portal, now);
        break;

    case UiState::Mining:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            APIs_FETCHER.enableFetching();
            enterState(UiState::SettingsScreen, now);
        } else {
            enterState(UiState::Mining, now);
        }
        break;
    case UiState::SettingsScreen:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::BTCScreen, now);
            APIs_FETCHER.enableFetching();
        }
        break;
    case UiState::BTCScreen:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::GlobalStats, now);
        }
        break;
    case UiState::GlobalStats:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::Mining, now);
        }
        break;
    case UiState::ShowQR:
        if (btn1Press || btn2Press) {
            // abort enrollment
            otp.disableEnrollment();
            enterState(UiState::Mining, now);
        }
        break;
    }
}



void DisplayDriver::waitForSplashs() {
    // wait until state is not Splash1 or Splash2
    while ((int) m_state < (int) UiState::Wait) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool DisplayDriver::ledControl(bool btn1, bool btn2) {
    // btn1 turns it on
    if (btn1) {
        return displayTurnOn();
    }

    // btn2 toggles the LED
    if (btn2) {
        if (!m_displayIsOn) {
            return displayTurnOn();
        }
        return displayTurnOff();
    }
    return false;
}

void DisplayDriver::lvglTimerTask(void *param)
{
    // Initial
    displayTurnOn();
    m_lastKeypressTime = now_us();
    enterState(UiState::Splash1, now_us());

    // button animation control
    int32_t elapsed_Ani_cycles = 0;

    Button btn1(PIN_BUTTON_1, 5000);
    Button btn2(PIN_BUTTON_2, 5000);

    ui_msg_t msg;

    while (true) {
        uint32_t wait_ms = 0;
        const int64_t tnow = now_us();
        {
            PThreadGuard lock(m_lvglMutex);

            increaseLvglTick();                  // typically lv_tick_inc(...)
            wait_ms = lv_timer_handler();        // time until next scheduled LVGL job
        }

        // 2) Decide sleep time outside the lock
        if (m_animationsEnabled) {
            // While animations are active, cap latency aggressively
            const uint32_t fast_cap = 5;         // ~200 FPS budget for responsiveness
            if (++elapsed_Ani_cycles > 80) {     // ~400 ms @ 5 ms cadence
                m_animationsEnabled = false;
                elapsed_Ani_cycles = 0;
            }
            // Sleep the smaller of LVGL's request and our fast cap
            uint32_t sleep_ms = (wait_ms > 0 && wait_ms < fast_cap) ? wait_ms : fast_cap;
            vTaskDelay(pdMS_TO_TICKS(sleep_ms));
            continue;
        }

        // Idle path: allow longer sleeps but keep a reasonable upper bound
        const uint32_t idle_cap = 50;            // latency cap while idle
        uint32_t sleep_ms = (wait_ms > 0 && wait_ms < idle_cap) ? wait_ms : idle_cap;
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));

        bool btn1Press = false;
        bool btn2Press = false;
        bool btnBothLongPress = false;

        btn1.update();
        btn2.update();

        uint32_t evt1 = btn1.getEvent();
        uint32_t evt2 = btn2.getEvent();

        // if we show the block found overlay, we hide it with any of the two buttons
        if ((evt1 & BTN_EVENT_SHORTPRESS || evt2 & BTN_EVENT_SHORTPRESS) && m_isActiveOverlay) {
            hideFoundBlockOverlay();
            btn1.clearEvent();
            btn2.clearEvent();
        }

        if ((evt1 & BTN_EVENT_LONGPRESS) && (evt2 & BTN_EVENT_LONGPRESS)) {
            btnBothLongPress = true;
            btn1.clearEvent();
            btn2.clearEvent();
        } else {
            if (evt1 & BTN_EVENT_SHORTPRESS) {
                m_lastKeypressTime = tnow;
                btn1Press = true;
                btn1.clearEvent();
            }

            if (evt2 & BTN_EVENT_SHORTPRESS) {
                m_lastKeypressTime = tnow;
                btn2Press = true;
                btn2.clearEvent();
            }
        }

        // queue to receive commands from http server context
        if (xQueueReceive(g_ui_queue, &msg, 0) == pdTRUE) {
            switch (msg.type) {
                case UI_CMD_SHOW_QR: {
                    if (!otp.isEnrollmentActive()) {
                        ESP_LOGE(TAG, "no otp enrollment active");
                        break;
                    }
                    int size = 0;
                    uint8_t* qrBuf = otp.getQrCode(&size);
                    m_ui->createQRScreen(qrBuf, size);
                    if (m_ui->ui_qrScreen) {
                        enterState(UiState::ShowQR, tnow);
                    }
                    m_isActiveOverlay = true;
                    break;
                }
                case UI_CMD_HIDE_QR: {
                    m_isActiveOverlay = false;
                    enterState(UiState::Mining, tnow);
                    break;
                }
            }
            if (msg.payload) {
                free(msg.payload);
                msg.payload = NULL;
            }
        }

        // Auto-Off / Overlay
        if (m_isActiveOverlay) {
            displayTurnOn();
        } else if (m_isAutoScreenOffEnabled) {
            checkAutoTurnOffScreen();
        }

        // 4) FSM update (timeouts and screen changes)
        updateState(tnow, btn1Press, btn2Press, btnBothLongPress);
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
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);

    esp_lcd_panel_swap_xy(panel_handle, true);

    Board *board = SYSTEM_MODULE.getBoard();
    if (!board->isFlipScreenEnabled()) {
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
    lv_color_t *buf1 = (lv_color_t*) MALLOC_DMA(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t));
    assert(buf1);
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

    float efficiency = power / (module->getCurrentHashrate() / 1000.0);
    float hashrate = module->getCurrentHashrate();

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
    PThreadGuard lock(m_lvglMutex);
    char strData[20];
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    Board *board = SYSTEM_MODULE.getBoard();

    lv_label_set_text(m_ui->ui_lbPoolSet, STRATUM_MANAGER.getCurrentPoolHost()); // Update label

    snprintf(strData, sizeof(strData), "%d", STRATUM_MANAGER.getCurrentPoolPort());
    lv_label_set_text(m_ui->ui_lbPortSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", board->getAsicFrequency());
    lv_label_set_text(m_ui->ui_lbFreqSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", board->getAsicVoltageMillis());
    lv_label_set_text(m_ui->ui_lbVcoreSet, strData); // Update label

    switch (m_tempControlMode) {
        case 1:
            lv_label_set_text(m_ui->ui_lbFanSet, "AUTO"); // Update label
            break;
        case 2:
            lv_label_set_text(m_ui->ui_lbFanSet, "PID"); // Update label
            break;
        default:
            snprintf(strData, sizeof(strData), "%d", m_fanSpeed);
            lv_label_set_text(m_ui->ui_lbFanSet, strData); // Update label
            break;
    }
}


void DisplayDriver::updateBTCprice(void)
{
    char price_str[32];

    if ((m_state != UiState::BTCScreen) && (m_btcPrice != 0))
        return;

    m_btcPrice = APIs_FETCHER.getPrice();
    snprintf(price_str, sizeof(price_str), "%u$", m_btcPrice);
    lv_label_set_text(m_ui->ui_lblBTCPrice, price_str); // Update label
}

void DisplayDriver::updateGlobalMiningStats(void)
{
    char strData[32];

    if ((m_state != UiState::GlobalStats) && (m_blockHeight != 0))
        return;


    m_blockHeight = APIs_FETCHER.getBlockHeight();
    snprintf(strData, sizeof(strData), "%lu", m_blockHeight);
    lv_label_set_text(m_ui->ui_lblBlock, strData); // Update label

    snprintf(strData, sizeof(strData), "%lu", APIs_FETCHER.getBlocksToHalving());
    lv_label_set_text(m_ui->ui_lblBlocksToHalving, strData); // Update label

    snprintf(strData, sizeof(strData), "%lu%%", APIs_FETCHER.getHalvingPercent());
    lv_label_set_text(m_ui->ui_lblHalvingPercent, strData); // Update label

    snprintf(strData, sizeof(strData), "%llu", APIs_FETCHER.getNetHash());
    lv_label_set_text(m_ui->ui_lblGlobalHash, strData); // Update label

    snprintf(strData, sizeof(strData), "%lluT", APIs_FETCHER.getNetDifficulty());
    lv_label_set_text(m_ui->ui_lblDifficulty, strData); // Update label

    snprintf(strData, sizeof(strData), "%lu", APIs_FETCHER.getLowestFee());
    lv_label_set_text(m_ui->ui_lbllowFee, strData); // Update label

    snprintf(strData, sizeof(strData), "%lu", APIs_FETCHER.getMidFee());
    lv_label_set_text(m_ui->ui_lblmedFee, strData); // Update label

    snprintf(strData, sizeof(strData), "%lu", APIs_FETCHER.getFastestFee());
    lv_label_set_text(m_ui->ui_lblhighFee, strData); // Update label
}

void DisplayDriver::updateGlobalState()
{
    PThreadGuard lock(m_lvglMutex);
    char strData[20];

    if (m_ui->ui_MiningScreen == NULL)
        return;
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    // snprintf(strData, sizeof(strData), "%.0f", power_management->chip_temp);
    snprintf(strData, sizeof(strData), "%.0f", POWER_MANAGEMENT_MODULE.getChipTempMax());
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
    updateGlobalMiningStats();

    Board *board = SYSTEM_MODULE.getBoard();
    uint16_t vcore = (int) (board->getVout() * 1000.0f);
    snprintf(strData, sizeof(strData), "%umV", vcore);
    lv_label_set_text(m_ui->ui_lbVcore, strData); // Update label
}

void DisplayDriver::updateIpAddress(const char *ip_address_str)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_ui->ui_MiningScreen == NULL)
        return;
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    lv_label_set_text(m_ui->ui_lbIP, ip_address_str);    // Update label
    lv_label_set_text(m_ui->ui_lbIPSet, ip_address_str); // Update label
}

void DisplayDriver::logMessage(const char *message)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_ui->ui_LogScreen == NULL)
        m_ui->logScreenInit();
    lv_label_set_text(m_ui->ui_LogLabel, message);
    enableLvglAnimations(true);
    _ui_screen_change(m_ui->ui_LogScreen, LV_SCR_LOAD_ANIM_NONE, 500, 0);
}

void DisplayDriver::miningScreen(void)
{
    PThreadGuard lock(m_lvglMutex);
    enterState(UiState::Mining, now_us());
}


void DisplayDriver::portalScreen(const char *message)
{
    PThreadGuard lock(m_lvglMutex);
    strlcpy(m_portalWifiName, message, sizeof(m_portalWifiName));
    lv_label_set_text(m_ui->ui_lbSSID, m_portalWifiName);
    enterState(UiState::Portal, now_us());
}

void DisplayDriver::updateWifiStatus(const char *message)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_ui->ui_lbConnect != NULL)
        lv_label_set_text(m_ui->ui_lbConnect, message); // Actualiza el label
    refreshScreen();
}

void DisplayDriver::buttonsInit(void)
{
    gpio_pad_select_gpio(PIN_BUTTON_1);
    gpio_set_direction(PIN_BUTTON_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_1, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(PIN_BUTTON_2);
    gpio_set_direction(PIN_BUTTON_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_2, GPIO_PULLUP_ONLY);
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

    // init the ipc
    ui_ipc_init();

    lv_obj_t *scr = initTDisplayS3();

    m_ui = new UI();
    m_ui->init(board);
    // manual_lvgl_update();

    // startUpdateScreenTask(); //Start screen update task
    mainCreatSysteTasks();
}
/**************************  Useful Electronics  ****************END OF FILE***/
