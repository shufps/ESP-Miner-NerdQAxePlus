
/**
 ******************************************************************************/

#ifndef DISPLAYDRIVER_H_
#define DISPLAYDRIVER_H_

#include "../global_state.h"
#include "../system.h"

/* INCLUDES ------------------------------------------------------------------*/

/* MACROS --------------------------------------------------------------------*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Inicializa el GPIO para el botón
#define PIN_BUTTON_1 14
#define PIN_BUTTON_2 0

#define TDISPLAYS3_LCD_PIXEL_CLOCK_HZ (6528000) // 170 (h) * 320 (w) * 2 (sizeof(lv_color_t)) * 60 (max fps)
#define TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL 1
#define TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL !TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL

#define TDISPLAYS3_PIN_NUM_DATA0 39
#define TDISPLAYS3_PIN_NUM_DATA1 40
#define TDISPLAYS3_PIN_NUM_DATA2 41
#define TDISPLAYS3_PIN_NUM_DATA3 42
#define TDISPLAYS3_PIN_NUM_DATA4 45
#define TDISPLAYS3_PIN_NUM_DATA5 46
#define TDISPLAYS3_PIN_NUM_DATA6 47
#define TDISPLAYS3_PIN_NUM_DATA7 48

#define TDISPLAYS3_PIN_RD GPIO_NUM_9
#define TDISPLAYS3_PIN_PWR 15
#define TDISPLAYS3_PIN_NUM_PCLK GPIO_NUM_8 // LCD_WR
#define TDISPLAYS3_PIN_NUM_CS 6
#define TDISPLAYS3_PIN_NUM_DC 7
#define TDISPLAYS3_PIN_NUM_RST 5
#define TDISPLAYS3_PIN_NUM_BK_LIGHT 38

// The pixel number in horizontal and vertical
#define TDISPLAYS3_LCD_H_RES 320
#define TDISPLAYS3_LCD_V_RES 170
#define LVGL_LCD_BUF_SIZE (TDISPLAYS3_LCD_H_RES * TDISPLAYS3_LCD_V_RES) / 4
// Bit number used to represent command and parameter
#define TDISPLAYS3_LCD_CMD_BITS 8
#define TDISPLAYS3_LCD_PARAM_BITS 8

// Supported alignment: 16, 32, 64.
// A higher alignment can enable higher burst transfer size, thus a higher i80 bus throughput.
#define LCD_PSRAM_TRANS_ALIGN 64
#define LCD_SRAM_TRANS_ALIGN 4

// Display screens status
#define STATE_ONINIT 0
#define STATE_SPLASH1 1
#define STATE_INIT_OK 2
#define SCREEN_PORTAL 3
#define SCREEN_MINING 4
#define SCREEN_BTCPRICE 5
#define SCREEN_SETTINGS 6
#define SCREEN_LOG 7

/* FUNCTIONS DECLARATION -----------------------------------------------------*/
void display_init(void);
void display_updateHashrate(SystemModule *module, float power);
void display_updateShares(SystemModule *module);
void display_updateTime(SystemModule *module);
void display_updateGlobalState(GlobalState *GLOBAL_STATE);
void display_updateCurrentSettings(GlobalState *GLOBAL_STATE);
void display_updateIpAddress(char *ip_address_str);
void enable_lvgl_animations(bool enable);
void display_RefreshScreen(void);
void display_log_message(const char *message);
void display_MiningScreen(void);
void display_PortalScreen(const char *message);
void display_UpdateWifiStatus(const char *message);

#endif /* DISPLAYDRIVER_H_ */
