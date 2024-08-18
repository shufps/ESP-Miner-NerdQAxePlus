
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lv_conf.h"
#include "APIs.h"
#include "../adc.h"
#include "esp_netif.h"
#include "TPS53647.h"
#include "nvs_config.h"
#include <inttypes.h>
//#include "../system.h"
//#include "lvgl.h"

#include "displayDriver.h"
#include "ui.h"
#include "ui_helpers.h"

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif


static const char *TAG = "TDisplayS3";
static bool animations_enabled = false;
static bool Button1Pressed_Flag = false;
static bool Button2Pressed_Flag = false;
static int64_t last_keypress_time = 0;
static bool DisplayIsOn = true;
static int screenStatus = STATE_ONINIT;
static int NextScreen = 0;
char portalWifiName[30];

static void example_increase_lvgl_tick			(void *arg);
static bool example_notify_lvgl_flush_ready		(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb				(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void main_creatSysteTasks				(void);
static void lvglTimerTask						(void* param);


static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/************ TURN ON/OFF DISPLAY FUCNTIONS  *************/
void display_turn_off(void) {
    // Power off Backlight
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);
    // Power off display power
    gpio_set_level(TDISPLAYS3_PIN_PWR, false);
    ESP_LOGI(TAG, "Screen off");
    DisplayIsOn = false;
}

void display_turn_on(void) {
    // Power on display power
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    // Power on Backlight
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Screen on");
    DisplayIsOn = true;
}

/************ AUTOTURN OFF DISPLAY FUCNTIONS  *************/
lv_obj_t * countdown_label;
static bool countdown_active = false;
static int64_t countdown_start_time = 0;

void start_countdown(void) {
    countdown_active = true;
    countdown_start_time = esp_timer_get_time();

    // Crear el recuadro negro y el rótulo si aún no existen
    if (countdown_label == NULL) {
        lv_obj_t *current_screen = lv_scr_act();
        lv_obj_t *black_box = lv_obj_create(current_screen);
        lv_obj_set_size(black_box, 200, 100);
        lv_obj_set_style_bg_color(black_box, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(black_box, 0, LV_PART_MAIN);
        lv_obj_align(black_box, LV_ALIGN_CENTER, 0, 0);

        countdown_label = lv_label_create(black_box);
        lv_label_set_text(countdown_label, "Turning screen off...");
        lv_obj_set_style_text_color(countdown_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(countdown_label);
    }
}


void display_hide_countdown(void) {
    if (countdown_label) {
        lv_obj_del(lv_obj_get_parent(countdown_label));
        countdown_label = NULL;
    }
}

void checkAutoTurnOffScreen(void) {

    if (DisplayIsOn == false) return;

    int64_t current_time = esp_timer_get_time();

    // Verificar si han pasado más de 2 minutos (120000000 microsegundos) desde la última pulsación de tecla
    if ((current_time - last_keypress_time) > 30000000) {
        if (!countdown_active) {
            start_countdown();
        }

        // Calcular el tiempo transcurrido desde que comenzó la cuenta regresiva
        int64_t elapsed_time = (current_time - countdown_start_time) / 1000000; // Convertir microsegundos a segundos

        //Turn off screen after 5 seconds of telling it
        if (elapsed_time > 5) {
            display_hide_countdown();
            display_turn_off();
            countdown_active = false;
        }

    } else {
        // Si se ha detectado actividad, cancelar la cuenta regresiva
        if (countdown_active) {
            display_hide_countdown();
            countdown_active = false;
        }
    }
}


static void increase_lvgl_tick()
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(TDISPLAYS3_LVGL_TICK_PERIOD_MS);
}

// Refresh screen values (for manual operations)
void display_RefreshScreen() {
    lv_timer_handler(); // Maneja las tareas pendientes de LVGL
    increase_lvgl_tick(); // Incrementa el tick según el periodo que definías antes
}

void changeScreen(void){ // * arg) {

    if(screenStatus == SCREEN_MINING) {
        enable_lvgl_animations(true); //AutoStops after loading the screen
        _ui_screen_change(ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        screenStatus = SCREEN_SETTINGS; // Actualiza la pantalla actual
        ESP_LOGI("UI", "NewScreen Settings displayed");
    } else if(screenStatus == SCREEN_SETTINGS) {
        enable_lvgl_animations(true); //AutoStops after loading the screen
        _ui_screen_change(ui_BTCScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        screenStatus = SCREEN_BTCPRICE; // Actualiza la pantalla actual
        ESP_LOGI("UI", "NewScreen Mining displayed");
    }else if(screenStatus == SCREEN_BTCPRICE){
        enable_lvgl_animations(true); //AutoStops after loading the screen
        _ui_screen_change(ui_MiningScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 350, 0);
        screenStatus = SCREEN_MINING; // Actualiza la pantalla actual
        ESP_LOGI("UI", "NewScreen BTCprice displayed");
    }

}

static void lvglTimerTask(void* param)
{
    int64_t myLastTime = esp_timer_get_time();
    uint8_t autoOffEnabled = nvs_config_get_u16(NVS_CONFIG_AUTO_SCREEN_OFF, 0);
    //int64_t current_time = esp_timer_get_time();

    //Check if screen is changing to avoid problems during change
    //if ((current_time - last_screen_change_time) < 1500000) return; // 1500000 microsegundos = 1500 ms = 1.5s - No cambies pantalla
    //last_screen_change_time = current_time;

    int32_t elapsed_Ani_cycles = 0;
	while(1) {

        //Enabled when change screen animation is activated
        if(animations_enabled) {
            increase_lvgl_tick();
            lv_timer_handler(); // Process pending LVGL tasks
            vTaskDelay(5 / portTICK_PERIOD_MS); // Delay during animations
            if(elapsed_Ani_cycles++>80) {
                //After 1s aprox stop animations
                animations_enabled = false;
                elapsed_Ani_cycles = 0;
            }
        }
        else{
            if(Button1Pressed_Flag) {
                Button1Pressed_Flag = false;
                last_keypress_time = esp_timer_get_time();
                if(!DisplayIsOn) display_turn_on();
                changeScreen();
            }
            vTaskDelay(200 / portTICK_PERIOD_MS); // Delay waiting animation trigger
        }
        if(Button2Pressed_Flag) {
            Button2Pressed_Flag = false;
            last_keypress_time = esp_timer_get_time();
            if(DisplayIsOn) display_turn_off();
            else display_turn_on();
        }

        //Check if screen need to be turned off
        if (autoOffEnabled) checkAutoTurnOffScreen();

        if((screenStatus > STATE_INIT_OK)) continue; //Doesn't need to do the initial animation screens

        //Screen initial process
        int32_t elapsed = (esp_timer_get_time() - myLastTime) / 1000;
        switch(screenStatus){
            case STATE_ONINIT: //First splash Screen
                                if(elapsed > 3000) {
                                    ESP_LOGI(TAG, "Changing Screen to SPLASH2");
                                    if (ui_Splash2 == NULL) ui_Splash2_screen_init();
                                    enable_lvgl_animations(true);
                                    _ui_screen_change(ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                                    screenStatus = STATE_SPLASH1;
                                    myLastTime = esp_timer_get_time();
                               }
                               break;
            case STATE_SPLASH1: //Second splash screen
                                if(elapsed > 3000) {
                                    //Init done, wait until on portal or mining is shown
                                    screenStatus = STATE_INIT_OK;
                                    ESP_LOGI(TAG, "Changing Screen to WAIT SELECTION");
                                    lv_obj_clean(ui_Splash1);
                                    ui_Splash1 = NULL;
                               }
                               break;
            case STATE_INIT_OK: //Show portal
                                if(NextScreen == SCREEN_PORTAL) {
                                    ESP_LOGI(TAG, "Changing Screen to Show Portal");
                                    screenStatus = SCREEN_PORTAL;
                                    if (ui_PortalScreen == NULL) {
                                        ui_Portal_screen_init();
                                    }
                                    lv_label_set_text(ui_lbSSID, portalWifiName); // Actualiza el label
                                    enable_lvgl_animations(true);
                                    _ui_screen_change(ui_PortalScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                                    lv_obj_clean(ui_Splash2);
                                    ui_Splash2 = NULL;
                                 }
                                else if(NextScreen == SCREEN_MINING){
                                    //Show Mining screen
                                    ESP_LOGI(TAG, "Changing Screen to Mining screen");
                                    screenStatus = SCREEN_MINING;
                                    if (ui_MiningScreen == NULL) ui_MiningScreen_screen_init();
                                    if (ui_SettingsScreen == NULL) ui_SettingsScreen_screen_init();
                                    if (ui_BTCScreen == NULL) ui_BTCScreen_screen_init();
                                    enable_lvgl_animations(true);
                                    _ui_screen_change(ui_MiningScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
                                    lv_obj_clean(ui_Splash2);
                                    ui_Splash2 = NULL;
                                 }
                               break;

        }
    }
}

// Función para activar las actualizaciones
void enable_lvgl_animations(bool enable) {
    animations_enabled = enable;
}

static void main_creatSysteTasks(void)
{

	xTaskCreatePinnedToCore(lvglTimerTask, "lvgl Timer", 6000, NULL, 4, NULL, 1); //Antes 10000
}

lv_obj_t * initTDisplayS3(void){
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    //GPIO configuration
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TDISPLAYS3_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

	gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
	gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
	gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);
    //esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
    //esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
    //esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);

	gpio_set_direction(TDISPLAYS3_PIN_RD, TDISPLAYS3_PIN_NUM_BK_LIGHT);
	gpio_set_direction(TDISPLAYS3_PIN_RD, GPIO_MODE_OUTPUT);
	gpio_set_direction(TDISPLAYS3_PIN_PWR, GPIO_MODE_OUTPUT);

    gpio_set_level(TDISPLAYS3_PIN_RD, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = TDISPLAYS3_PIN_NUM_DC,
        .wr_gpio_num = TDISPLAYS3_PIN_NUM_PCLK,
		.clk_src	= LCD_CLK_SRC_DEFAULT,
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
        .sram_trans_align = LCD_SRAM_TRANS_ALIGN
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = TDISPLAYS3_PIN_NUM_CS,
        .pclk_hz = TDISPLAYS3_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = TDISPLAYS3_LCD_CMD_BITS,
        .lcd_param_bits = TDISPLAYS3_LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install LCD driver of st7789");
    esp_lcd_panel_handle_t panel_handle = NULL;

    esp_lcd_panel_dev_config_t panel_config =
    {
        .reset_gpio_num = TDISPLAYS3_PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);


    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);

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
    lv_color_t *buf1 = heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA );
    assert(buf1);
//    lv_color_t *buf2 = heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA );
//    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LVGL_LCD_BUF_SIZE);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TDISPLAYS3_LCD_H_RES;
    disp_drv.ver_res = TDISPLAYS3_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    //Configuration is completed.


    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    /*const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };*/
    esp_timer_handle_t lvgl_tick_timer = NULL;
    //ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    //ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, TDISPLAYS3_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL animation");
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    return scr;
}

void display_updateHashrate(SystemModule * module, float power){
    char strData[20];

    float efficiency = power / (module->current_hashrate / 1000.0);

    snprintf(strData, sizeof(strData), "%.1f", module->current_hashrate);
    lv_label_set_text(ui_lbHashrate, strData); // Update hashrate
    lv_label_set_text(ui_lbHashrateSet, strData); // Update hashrate
    lv_label_set_text(ui_lblHashPrice, strData); // Update hashrate

    snprintf(strData, sizeof(strData), "%.1f", efficiency);
    lv_label_set_text(ui_lbEficiency, strData); // Update eficiency label

    snprintf(strData, sizeof(strData), "%.3fW", power);
    lv_label_set_text(ui_lbPower, strData); // Actualiza el label

}

void display_updateShares(SystemModule * module){
    char strData[20];

    snprintf(strData, sizeof(strData), "%lld/%lld", module->shares_accepted, module->shares_rejected);
    lv_label_set_text(ui_lbShares, strData); // Update shares

    snprintf(strData, sizeof(strData), "%s", module->best_diff_string);
    lv_label_set_text(ui_lbBestDifficulty, module->best_diff_string); // Update Bestdifficulty
    lv_label_set_text(ui_lbBestDifficultySet, module->best_diff_string); // Update Bestdifficulty

}
void display_updateTime(SystemModule * module){
    char strData[20];

    // Calculate the uptime in seconds
    //int64_t currentTimeTest = esp_timer_get_time() + (8 * 3600 * 1000000LL) + (1800 * 1000000LL);//(8 * 60 * 60 * 10000);
    double uptime_in_seconds = (esp_timer_get_time() - module->start_time) / 1000000;
    int uptime_in_days = uptime_in_seconds / (3600 * 24);
    int remaining_seconds = (int) uptime_in_seconds % (3600 * 24);
    int uptime_in_hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    int uptime_in_minutes = remaining_seconds / 60;
    int current_seconds = remaining_seconds % 60;

    snprintf(strData, sizeof(strData), "%dd %ih %im %is", uptime_in_days, uptime_in_hours, uptime_in_minutes, current_seconds);
    lv_label_set_text(ui_lbTime, strData); // Update label

}

void display_updateCurrentSettings(GlobalState * GLOBAL_STATE){
    char strData[20];
    if(ui_SettingsScreen == NULL)  return;

    lv_label_set_text(ui_lbPoolSet, GLOBAL_STATE->SYSTEM_MODULE.pool_url); // Update label

    snprintf(strData, sizeof(strData), "%d", GLOBAL_STATE->SYSTEM_MODULE.pool_port);
    lv_label_set_text(ui_lbPortSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY));
    lv_label_set_text(ui_lbFreqSet, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE));
    lv_label_set_text(ui_lbVcoreSet, strData); // Update label

    uint16_t auto_fan_speed = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1);
    if(auto_fan_speed == 1) lv_label_set_text(ui_lbFanSet, "AUTO"); // Update label
    else {
        snprintf(strData, sizeof(strData), "%d", nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100));
        lv_label_set_text(ui_lbFanSet, strData); // Update label
    }

}

unsigned int BTCprice = 0;
void display_updateBTCprice(void){
    char price_str[32];

    if ((screenStatus != SCREEN_BTCPRICE)&&(BTCprice != 0)) return;

    BTCprice = getBTCprice();
    snprintf(price_str, sizeof(price_str), "%u$", BTCprice);
    lv_label_set_text(ui_lblBTCPrice, price_str); // Update label
}

void display_updateGlobalState(GlobalState * GLOBAL_STATE){
    char strData[20];

    if(ui_MiningScreen == NULL)  return;
    if(ui_SettingsScreen == NULL)  return;

    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    //snprintf(strData, sizeof(strData), "%.0f", power_management->chip_temp);
    snprintf(strData, sizeof(strData), "%.0f", power_management->chip_temp_avg);
    lv_label_set_text(ui_lbTemp, strData); // Update label
    lv_label_set_text(ui_lblTempPrice, strData); // Update label

    snprintf(strData, sizeof(strData), "%d", power_management->fan_rpm);
    lv_label_set_text(ui_lbRPM, strData); // Update label

    snprintf(strData, sizeof(strData), "%.3fW", power_management->power);
    lv_label_set_text(ui_lbPower, strData); // Update label

    snprintf(strData, sizeof(strData), "%imA", (int) power_management->current);
    lv_label_set_text(ui_lbIntensidad, strData); // Update label

    snprintf(strData, sizeof(strData), "%imV", (int) power_management->voltage);
    lv_label_set_text(ui_lbVinput, strData); // Update label

    display_updateTime(module);
    display_updateShares(module);
    display_updateHashrate(module, GLOBAL_STATE->POWER_MANAGEMENT_MODULE.power);
    display_updateBTCprice();

    //uint16_t vcore = ADC_get_vcore(); TODO
    uint16_t vcore = (int) (TPS53647_get_vout() * 1000.0f);
    snprintf(strData, sizeof(strData), "%umV", vcore);
    lv_label_set_text(ui_lbVcore, strData); // Update label
}

void display_updateIpAddress(char * ip_address_str){
    char strData[20];

    if(ui_MiningScreen == NULL)  return;
    if(ui_SettingsScreen == NULL)  return;

    snprintf(strData, sizeof(strData), "%s", ip_address_str);
    lv_label_set_text(ui_lbIP, ip_address_str); // Update label
    lv_label_set_text(ui_lbIPSet, ip_address_str); // Update label

}

void display_log_message(const char *message)
{
    screenStatus = SCREEN_LOG;
    if(ui_LogScreen == NULL) ui_LogScreen_init();
    lv_label_set_text(ui_LogLabel, message);
    enable_lvgl_animations(true);
    _ui_screen_change(ui_LogScreen, LV_SCR_LOAD_ANIM_NONE, 500, 0);
}

void display_MiningScreen(void)
{
    //Only called once at the beggining from system lib
    if (ui_MiningScreen == NULL) ui_MiningScreen_screen_init();
    if (ui_SettingsScreen == NULL) ui_SettingsScreen_screen_init();
    if (ui_BTCScreen == NULL) ui_BTCScreen_screen_init();
    NextScreen = SCREEN_MINING;
}

void display_PortalScreen(const char *message)
{
    NextScreen = SCREEN_PORTAL;
    strcpy(portalWifiName, message);
}
void display_UpdateWifiStatus(const char *message)
{
    if(ui_lbConnect != NULL)
        lv_label_set_text(ui_lbConnect, message); // Actualiza el label
    display_RefreshScreen();

}

// ISR Handler para el DownButton (Change Screen)
static void button1_isr_handler(void* arg) {
    //ESP_LOGI("UI", "Button pressed changing screen");
    Button1Pressed_Flag=true;
}

// ISR Handler para el UpButton (Change Screen)
static void button2_isr_handler(void* arg) {
    Button2Pressed_Flag=true;
}

void buttons_init(void) {
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
    gpio_isr_handler_add(PIN_BUTTON_1, button1_isr_handler, NULL);
    gpio_isr_handler_add(PIN_BUTTON_2, button2_isr_handler, NULL);
}

/**
 * @brief Program starts from here
 *
 */
void display_init(void)
{
    ESP_LOGI("INFO", "Setting Up TDisplayS3 Screen");

    // Inicializa el GPIO para el botón
    buttons_init();

    lv_obj_t * scr = initTDisplayS3();

    ui_init();
    //manual_lvgl_update();

    //startUpdateScreenTask(); //Start screen update task
    main_creatSysteTasks();

}
/**************************  Useful Electronics  ****************END OF FILE***/
