# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

#add_definitions(-DDISPLAY_OLED)
add_definitions(-DDISPLAY_TTGO)

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

