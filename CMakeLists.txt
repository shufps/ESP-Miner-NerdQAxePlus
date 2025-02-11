# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
#cmake_minimum_required(VERSION 3.5)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

# Check if the environment variable BOARD is defined
if(DEFINED ENV{BOARD})
    add_compile_definitions($ENV{BOARD})
else()
    message(FATAL_ERROR "Environment variable BOARD is not set")
endif()

message(STATUS "Selected board: $ENV{BOARD}")

add_compile_definitions(LV_MEM_CUSTOM=1)
add_compile_definitions(LV_MEM_CUSTOM_INCLUDE=<lvgl_psram.h>)
add_compile_definitions(LV_MEM_CUSTOM_ALLOC=lv_psram_alloc)
add_compile_definitions(LV_MEM_CUSTOM_FREE=lv_psram_free)
add_compile_definitions(LV_MEM_CUSTOM_REALLOC=lv_psram_realloc)

# not longer used
#add_definitions(-DDISPLAY_TTGO)

#enable memory tracker
#add_definitions(-DDEBUG_MEMORY_LOGGING)

# adds -I./main; needed for components to have
# access to main include files
include_directories("./main")

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(esp-miner)

