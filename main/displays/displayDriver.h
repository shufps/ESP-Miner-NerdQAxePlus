#pragma once

//#include "../global_state.h"

#include "../boards/board.h"
#include "esp_lcd_panel_io.h"
#include "ui.h"
#include "ui_helpers.h"

/* INCLUDES ------------------------------------------------------------------*/

/* MACROS --------------------------------------------------------------------*/

// GPIO for buttons
#define PIN_BUTTON_1 (gpio_num_t) 14 // Button 1 GPIO pin
#define PIN_BUTTON_2 (gpio_num_t) 0  // Button 2 GPIO pin

// Display settings
#define TDISPLAYS3_LCD_PIXEL_CLOCK_HZ (6528000)                             // Pixel clock for LCD in Hz (60 FPS, 170 x 320 pixels)
#define TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL 1                                  // Backlight ON level (1: ON, 0: OFF)
#define TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL !TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL // Backlight OFF level

// GPIO pin numbers for LCD data lines
#define TDISPLAYS3_PIN_NUM_DATA0 (gpio_num_t) 39
#define TDISPLAYS3_PIN_NUM_DATA1 (gpio_num_t) 40
#define TDISPLAYS3_PIN_NUM_DATA2 (gpio_num_t) 41
#define TDISPLAYS3_PIN_NUM_DATA3 (gpio_num_t) 42
#define TDISPLAYS3_PIN_NUM_DATA4 (gpio_num_t) 45
#define TDISPLAYS3_PIN_NUM_DATA5 (gpio_num_t) 46
#define TDISPLAYS3_PIN_NUM_DATA6 (gpio_num_t) 47
#define TDISPLAYS3_PIN_NUM_DATA7 (gpio_num_t) 48

// Other LCD control pins
#define TDISPLAYS3_PIN_RD GPIO_NUM_9                // LCD read pin
#define TDISPLAYS3_PIN_PWR (gpio_num_t) 15          // LCD power control pin
#define TDISPLAYS3_PIN_NUM_PCLK GPIO_NUM_8          // LCD pixel clock (WR) pin
#define TDISPLAYS3_PIN_NUM_CS (gpio_num_t) 6        // LCD chip select pin
#define TDISPLAYS3_PIN_NUM_DC (gpio_num_t) 7        // LCD data/command pin
#define TDISPLAYS3_PIN_NUM_RST (gpio_num_t) 5       // LCD reset pin
#define TDISPLAYS3_PIN_NUM_BK_LIGHT (gpio_num_t) 38 // LCD backlight control pin

// LCD resolution and buffer size
#define TDISPLAYS3_LCD_H_RES 320                                            // Horizontal resolution
#define TDISPLAYS3_LCD_V_RES 170                                            // Vertical resolution
#define LVGL_LCD_BUF_SIZE (TDISPLAYS3_LCD_H_RES * TDISPLAYS3_LCD_V_RES) / 4 // Buffer size for display

// Bit sizes for LCD commands and parameters
#define TDISPLAYS3_LCD_CMD_BITS 8   // Bits for LCD commands
#define TDISPLAYS3_LCD_PARAM_BITS 8 // Bits for LCD parameters

// Alignment settings for PSRAM and SRAM transfers
#define LCD_PSRAM_TRANS_ALIGN 64 // Alignment for PSRAM transfers
#define LCD_SRAM_TRANS_ALIGN 4   // Alignment for SRAM transfers

// Display screen states
#define STATE_ONINIT 0    // Initial screen state
#define STATE_SPLASH1 1   // First splash screen
#define STATE_INIT_OK 2   // Initialization complete
#define SCREEN_PORTAL 3   // Portal screen
#define SCREEN_MINING 4   // Mining screen
#define SCREEN_BTCPRICE 5 // BTC price screen
#define SCREEN_SETTINGS 6 // Settings screen
#define SCREEN_LOG 7      // Log screen
#define SCREEN_GLBSTATS 8 // Global Mining stats screen

/* CLASS DECLARATION -----------------------------------------------------*/
class System;

class DisplayDriver {
  public:
    // display state machine
    enum class UiState : uint8_t {
        Splash1,
        Splash2,
        Wait,
        Portal,
        Mining
    };

  protected:
    UiState m_state = UiState::Splash1;
    int64_t m_stateStart_us = 0;

    pthread_mutex_t m_lvglMutex;
    bool m_animationsEnabled;   // Flag for enabling animations
    bool m_button1PressedFlag;  // Flag indicating button 1 is pressed
    bool m_button2PressedFlag;  // Flag indicating button 2 is pressed
    int64_t m_lastKeypressTime; // Time of the last keypress event
    bool m_displayIsOn;         // Flag indicating if the display is currently on
    int m_screenStatus;         // Current screen status
    int m_nextScreen;           // The next screen to display
    bool m_isActiveOverlay;     // flag if we have an overlay. LED light is forced to be on
    char m_portalWifiName[30];  // WiFi name displayed on the portal screen

    lv_obj_t *m_countdownLabel = nullptr; // Label object for the countdown timer
    bool m_countdownActive = false;       // Flag for countdown timer activity
    int64_t m_countdownStartTime = 0;     // Start time for the countdown

    unsigned int m_btcPrice; // Current Bitcoin price
    uint32_t m_blockHeight; // Current Bitcoin price

    UI *m_ui;

    // Helper methods for LVGL handling
    static bool notifyLvglFlushReady(esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t *edata, void *userCtx);
    static void lvglFlushCallback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorMap);

    // Enables or disables animations for LVGL
    void enableLvglAnimations(bool enable);

    void updateBTCprice(void);

    // Display-related functions
    void displayTurnOff();         // Turn off the display
    void displayTurnOn();          // Turn on the display
    void startCountdown();         // Start the screen countdown timer
    void displayHideCountdown();   // Hide the countdown overlay
    void checkAutoTurnOffScreen(); // Check if the screen should auto-turn off
    void increaseLvglTick();       // Increments the LVGL tick counter

    // Button initialization and handling
    void buttonsInit();                    // Initialize GPIO buttons
    static void button1IsrHandler(void *); // ISR handler for button 1
    static void button2IsrHandler(void *); // ISR handler for button 2

    // Screen switching logic
    void changeScreen();   // Change between screens
    void updateBtcPrice(); // Update Bitcoin price on the screen
    void updateGlobalMiningStats(); // Update Global mining stats

    // LVGL task handling
    void mainCreatSysteTasks();                    // Creates system tasks for LVGL
    static void lvglTimerTaskWrapper(void *param); // Wrapper for LVGL timer task
    void lvglTimerTask(void *param);               // LVGL timer task implementation

    // display state machine
    void enterState(UiState s, int64_t now);
    void updateState(int64_t now);

    // Display initialization
    lv_obj_t *initTDisplayS3(); // Initialize the TDisplay S3

    void updateHashrate(System *module, float power);               // Update the hashrate display
    void updateShares(System *module);                              // Update the shares information on the display
    void updateTime(System *module);                                // Update the time display
    void lvglAnimations(bool enable);                               // Enable or disable LVGL animations

    void hideFoundBlockOverlay();

  public:
    // Constructor
    DisplayDriver();
    // Public methods
    void init(Board *board);                                        // Initialize the display system
    void updateWifiStatus(const char *message);                     // Update the WiFi status on the display
    void portalScreen(const char *message);                         // Switch to the portal screen
    void showError(const char *error_message, uint32_t error_code); // Show generic error
    void hideError();                                               // Hide error overlay
    void miningScreen();                                            // Switch to the mining screen
    void updateIpAddress(const char *ipAddressStr);                 // Update the displayed IP address
    void showFoundBlockOverlay();
    void updateGlobalState();                                       // Update the global state on the display
    void updateCurrentSettings();                                   // Update the current settings screen
    void refreshScreen();                                           // Refresh the display
    void logMessage(const char *message);                           // Log a message to the display
    void waitForSplashs();

    UiState getState() {
      return m_state;
    }
};
