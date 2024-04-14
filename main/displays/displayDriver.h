
/**
 ******************************************************************************/

#ifndef DISPLAYDRIVER_H_
#define DISPLAYDRIVER_H_

#include "../system.h"
#include "../global_state.h"

/* INCLUDES ------------------------------------------------------------------*/

/* MACROS --------------------------------------------------------------------*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Inicializa el GPIO para el botón
#define PIN_BUTTON_1 14
#define PIN_BUTTON_2 0

#define TDISPLAYS3_LCD_PIXEL_CLOCK_HZ     (6528000) // 170 (h) * 320 (w) * 2 (sizeof(lv_color_t)) * 60 (max fps)
#define TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL  1
#define TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL !TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL

#define TDISPLAYS3_PIN_NUM_DATA0          39
#define TDISPLAYS3_PIN_NUM_DATA1          40
#define TDISPLAYS3_PIN_NUM_DATA2          41
#define TDISPLAYS3_PIN_NUM_DATA3          42
#define TDISPLAYS3_PIN_NUM_DATA4          45
#define TDISPLAYS3_PIN_NUM_DATA5          46
#define TDISPLAYS3_PIN_NUM_DATA6          47
#define TDISPLAYS3_PIN_NUM_DATA7          48

#define TDISPLAYS3_PIN_RD          	   GPIO_NUM_9
#define TDISPLAYS3_PIN_PWR          	   15
#define TDISPLAYS3_PIN_NUM_PCLK           GPIO_NUM_8		//LCD_WR
#define TDISPLAYS3_PIN_NUM_CS             6
#define TDISPLAYS3_PIN_NUM_DC             7
#define TDISPLAYS3_PIN_NUM_RST            5
#define TDISPLAYS3_PIN_NUM_BK_LIGHT       38

// The pixel number in horizontal and vertical
#define TDISPLAYS3_LCD_H_RES              320
#define TDISPLAYS3_LCD_V_RES              170
#define LVGL_LCD_BUF_SIZE            (TDISPLAYS3_LCD_H_RES * TDISPLAYS3_LCD_V_RES)
// Bit number used to represent command and parameter
#define TDISPLAYS3_LCD_CMD_BITS           8
#define TDISPLAYS3_LCD_PARAM_BITS         8

#define TDISPLAYS3_LVGL_TICK_PERIOD_MS    2

/* FUNCTIONS DECLARATION -----------------------------------------------------*/
void display_init(void);
void display_updateHashrate(SystemModule * module, float power);
void display_updateShares(SystemModule * module);
void display_updateGlobalState(GlobalState * GLOBAL_STATE);
void display_updateIpAddress(char * ip_address_str);



#endif /* DISPLAYDRIVER_H_ */

