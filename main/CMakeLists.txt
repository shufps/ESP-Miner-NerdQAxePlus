idf_component_register(
SRCS
    "EMC2302.c"

    "i2c_master.c"

    "main.c"
    "nvs_config.c"
    "system.c"
    "TMP1075.c"
    "TPS53647.c"
    "vcore.c"
    "leak_tracker.c"
    "history.c"

    "./http_server/http_server.c"
    "./self_test/self_test.c"
    "./tasks/stratum_task.c"
    "./tasks/create_jobs_task.c"
    "./tasks/asic_result_task.c"
    "./tasks/user_input_task.c"
    "./tasks/influx_task.c"
    "./tasks/power_management_task.c"
    "./http_server/http_server.c"
    "./self_test/self_test.c"
    "./displays/displayDriver.c"
    "./displays/ui.c"
    "./displays/ui_helpers.c"
    "./displays/APIs.c"
    "./displays/images/ui_font_DigitalNumbers16.c"
    "./displays/images/ui_font_DigitalNumbers28.c"
    "./displays/images/ui_font_DigitalNumbers16.c"
    "./displays/images/ui_font_OpenSansBold13.c"
    "./displays/images/ui_font_OpenSansBold14.c"
    "./displays/images/ui_font_OpenSansBold24.c"
    "./displays/images/ui_font_OpenSansBold45.c"
    "./displays/images/ui_img_initscreen2_png.c"
    "./displays/images/ui_img_miningscreen2_png.c"
    "./displays/images/ui_img_settingsscreen_png.c"
    "./displays/images/ui_img_splashscreen2_png.c"
    "./displays/images/ui_img_portalscreen_png.c"
    "./displays/images/ui_img_btcscreen_png.c"
    "./displays/images/ui_img_overheat_png.c"


INCLUDE_DIRS
    "."
    "tasks"
    "http_server"
    "displays"
    "displays/images"
    "../components/connect/include"
    "../components/dns_server/include"
    "../components/bm1397/include"
    "../components/influx/include"
    "../components/stratum/include"

    PRIV_REQUIRES
    "app_update"
    "driver"
    "esp_adc"
    "esp_app_format"
    "esp_event"
    "esp_http_server"
    "esp_http_client"
    "esp_netif"
    "esp_timer"
    "esp_wifi"
    "esp_lcd"
    "esp_psram"
    "json"
    "nvs_flash"
    "spiffs"
    "vfs"
    "lvgl"
)


set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/http_server/axe-os")

if("$ENV{GITHUB_ACTIONS}" STREQUAL "true")
    message(STATUS "Running on GitHub Actions. Web ui will be prebuilt.")

    spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist/axe-os FLASH_IN_PROJECT)
else()
    find_program(NPM_EXECUTABLE npm)
    if(NOT NPM_EXECUTABLE AND NOT EXISTS ${WEB_SRC_DIR}/dist)
        message(FATAL_ERROR "npm is not found! Please install it to proceed.")
    endif()

    ExternalProject_Add(
        web_ui_dist
        PREFIX ${CMAKE_BINARY_DIR}/web_ui_dist
        SOURCE_DIR ${WEB_SRC_DIR}
        DOWNLOAD_COMMAND ""
        CONFIGURE_COMMAND
            ${NPM_EXECUTABLE}  i
        USES_TERMINAL_BUILD true
        BUILD_COMMAND
            ${NPM_EXECUTABLE} run build
        INSTALL_COMMAND ""
        BUILD_ALWAYS OFF
        BUILD_IN_SOURCE TRUE
        BUILD_BYPRODUCTS
            "${WEB_SRC_DIR}/dist/axe-os/index.html"
    )

    add_dependencies(${COMPONENT_LIB} web_ui_dist)

    spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist/axe-os FLASH_IN_PROJECT DEPENDS web_ui_dist)
endif()