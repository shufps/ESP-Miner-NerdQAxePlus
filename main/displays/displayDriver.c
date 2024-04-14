
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
#include "../adc.h"
#include "esp_netif.h"
//#include "../system.h"
//#include "lvgl.h"

#include "displayDriver.h"
#include "ui.h"

static const char *TAG = "example";



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

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(TDISPLAYS3_LVGL_TICK_PERIOD_MS);
}

static void lvglTimerTask(void* param)
{
	while(1)
	{
		// The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
		lv_timer_handler();

		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}

static void main_creatSysteTasks(void)
{

	xTaskCreatePinnedToCore(lvglTimerTask, "lvgl Timer", 10000, NULL, 4, NULL, 1);
}

lv_obj_t * initTDisplayS3(void){
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    //GPIO configuration
    ESP_LOGI(TAG, "TDisplayS3 - Turn off LCD backlight");
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

    ESP_LOGI(TAG, "TDisplayS3 - Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = TDISPLAYS3_PIN_NUM_DC,
        .wr_gpio_num = TDISPLAYS3_PIN_NUM_PCLK,
		.clk_src	= LCD_CLK_SRC_PLL160M,
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
        .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t)
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

    ESP_LOGI(TAG, "TDisplayS3 - Install LCD driver of st7789");
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
    esp_lcd_panel_mirror(panel_handle, false, true);
    // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
    esp_lcd_panel_set_gap(panel_handle, 0, 35);



    ESP_LOGI(TAG, "TDisplayS3 - Turn on LCD backlight");
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);



    ESP_LOGI(TAG, "TDisplayS3 - Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA );
    assert(buf1);
//    lv_color_t *buf2 = heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA );
//    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LVGL_LCD_BUF_SIZE);

    ESP_LOGI(TAG, "TDisplayS3 - Register display driver to LVGL");
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
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, TDISPLAYS3_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL animation");
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    return scr;
}

void display_updateHashrate(SystemModule * module, float power){
    char strData[20];

    float efficiency = power / (module->current_hashrate / 1000.0);

    sprintf(strData, "%.1f", module->current_hashrate);
    lv_label_set_text(ui_lbHashrate, strData); // Update hashrate
    lv_label_set_text(ui_lbHashrateSet, strData); // Update hashrate

    sprintf(strData, "%.1f", efficiency);
    lv_label_set_text(ui_lbEficiency, strData); // Update eficiency label
    
    sprintf(strData, "%.3fW", power);
    lv_label_set_text(ui_lbPower, strData); // Actualiza el label

}

void display_updateShares(SystemModule * module){
    char strData[20];
    
    sprintf(strData, "%d/%d", module->shares_accepted, module->shares_rejected);
    lv_label_set_text(ui_lbShares, strData); // Update shares

    lv_label_set_text(ui_lbBestDifficulty, module->best_diff_string); // Update Bestdifficulty
    lv_label_set_text(ui_lbBestDifficultySet, module->best_diff_string); // Update Bestdifficulty

}

void display_updateGlobalState(GlobalState * GLOBAL_STATE){
    char strData[20];

    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    // Calculate the uptime in seconds
    double uptime_in_seconds = (esp_timer_get_time() - module->start_time) / 1000000;
    int uptime_in_days = uptime_in_seconds / (3600 * 24);
    int remaining_seconds = (int) uptime_in_seconds % (3600 * 24);
    int uptime_in_hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    int uptime_in_minutes = remaining_seconds / 60;

    sprintf(strData, "%dd %ih %im", uptime_in_days, uptime_in_hours, uptime_in_minutes);
    lv_label_set_text(ui_lbTime, strData); // Update label

    sprintf(strData, "%.0f", power_management->chip_temp);
    lv_label_set_text(ui_lbTemp, strData); // Update label

    sprintf(strData, "%d", power_management->fan_speed);
    lv_label_set_text(ui_lbRPM, strData); // Update label

    sprintf(strData, "%.3fW", power_management->power);
    lv_label_set_text(ui_lbPower, strData); // Update label

    sprintf(strData, "%imA", (int) power_management->current);
    lv_label_set_text(ui_lbIntensidad, strData); // Update label

    sprintf(strData, "%imV", (int) power_management->voltage);
    lv_label_set_text(ui_lbVinput, strData); // Update label

    display_updateShares(module);
    display_updateHashrate(module, GLOBAL_STATE->POWER_MANAGEMENT_MODULE.power);

    uint16_t vcore = ADC_get_vcore();
    sprintf(strData, "%umV", vcore);
    lv_label_set_text(ui_lbVcore, strData); // Update label

}

void display_updateIpAddress(char * ip_address_str){
    lv_label_set_text(ui_lbIP, ip_address_str); // Update label
    lv_label_set_text(ui_lbIPSet, ip_address_str); // Update label
}

/*
void updateScreensTask(void *pvParameter) {
    int hash = 0;
    
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1000)); // Espera un segundo

        //lv_label_set_text(ui_lbTime,
    }
}

void startUpdateScreenTask() {
    xTaskCreate(updateScreensTask, "updateScreensTask", 2048, NULL, 5, NULL);
}*/



// ISR Handler para el botón
static void boton_isr_handler(void* arg) {
    //ESP_LOGI("UI", "Button pressed changing screen");
    lv_async_call(changeScreen, NULL);
}

void buttons_init(void) {
    gpio_pad_select_gpio(PIN_BUTTON_1);
    gpio_set_direction(PIN_BUTTON_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_1, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_1, GPIO_INTR_POSEDGE); // Interrupción en flanco de bajada

    // Habilita las interrupciones de GPIO
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON_1, boton_isr_handler, NULL);
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

    //startUpdateScreenTask(); //Start screen update task
    main_creatSysteTasks();

}
/**************************  Useful Electronics  ****************END OF FILE***/
