idf_component_register(
SRCS
    "main.cpp"
    "nvs_config.cpp"
    "system.cpp"
    "sntp.cpp"
    "boards/board.cpp"
    "boards/nerdaxe.cpp"
    "boards/nerdaxegamma.cpp"
    "boards/nerdqaxeplus.cpp"
    "boards/nerdqaxeplus2.cpp"
    "boards/nerdoctaxeplus.cpp"
    "boards/nerdoctaxegamma.cpp"
    "boards/nerdhaxegamma.cpp"
    "boards/nerdqx.cpp"
    "boards/nerdeko.cpp"
    "boards/drivers/EMC2302.cpp"
    "boards/drivers/TMP1075.cpp"
    "boards/drivers/TPS53647.cpp"
    "boards/drivers/TPS53667.cpp"
    "boards/drivers/nerdaxe/DS4432U.cpp"
    "boards/drivers/nerdaxe/INA260.cpp"
    "boards/drivers/nerdaxe/EMC2101.cpp"
    "boards/drivers/nerdaxe/TPS546.cpp"
    "boards/drivers/nerdaxe/adc.cpp"
    "boards/drivers/i2c_master.cpp"
    "boards/drivers/tmp451_mux.cpp"
    "history.cpp"
    "discord.cpp"
    "./pid/PID_v1_bc.cpp"
    "./pid/pid_timer.cpp"
    "./http_server/http_server.cpp"
    "./http_server/http_cors.cpp"
    "./http_server/http_utils.cpp"
    "./http_server/http_websocket.cpp"
    "./http_server/handler_influx.cpp"
    "./http_server/handler_alert.cpp"
    "./http_server/handler_swarm.cpp"
    "./http_server/handler_system.cpp"
    "./http_server/handler_ota.cpp"
    "./http_server/handler_otp.cpp"
    "./http_server/handler_restart.cpp"
    "./http_server/handler_file.cpp"
    "./http_server/handler_ota_factory.cpp"
    "./self_test/self_test.cpp"
    "./stratum/stratum_task.cpp"
    "./stratum/stratum_manager.cpp"
    "./stratum/stratum_manager_fallback.cpp"
    "./stratum/stratum_manager_dual_pool.cpp"
    "./tasks/create_jobs_task.cpp"
    "./tasks/asic_result_task.cpp"
    "./tasks/influx_task.cpp"
    "./tasks/ping_task.cpp"
    "./tasks/power_management_task.cpp"
    "./tasks/hashrate_monitor_task.cpp"
    "./tasks/apis_task.cpp"
    "./tasks/wifi_health.cpp"
    "./displays/displayDriver.cpp"
    "./displays/ui.cpp"
    "./displays/ui_ipc.cpp"
    "./displays/button.cpp"
    "./displays/ui_helpers.cpp"
    "./displays/images/ui_font_DigitalNumbers16.c"
    "./displays/images/ui_font_DigitalNumbers28.c"
    "./displays/images/ui_font_DigitalNumbers16.c"
    "./displays/images/ui_font_OpenSansBold13.c"
    "./displays/images/ui_font_OpenSansBold14.c"
    "./displays/images/ui_font_OpenSansBold24.c"
    "./displays/images/ui_font_OpenSansBold45.c"
    "./displays/images/ui_font_vt323_21.c"
    "./displays/images/ui_font_vt323_35.c"
    "./displays/images/ui_img_found_block_png.c"
    "./displays/images/ui_img_safe_png.c"
    "./displays/images/themes/themes.c"
    "./otp/otp.cpp"
    "./otp/qrcodegen.cpp"



INCLUDE_DIRS
    "."
    "tasks"
    "http_server"
    "displays"
    "displays/images"
    "boards"
    "boards/drivers"
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
    "esp_https_ota"
    "esp_netif"
    "esp_timer"
    "esp-tls"
    "esp_wifi"
    "esp_lcd"
    "esp_psram"
    "json"
    "nvs_flash"
    "spiffs"
    "vfs"
    "lvgl"
    "lwip"
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
