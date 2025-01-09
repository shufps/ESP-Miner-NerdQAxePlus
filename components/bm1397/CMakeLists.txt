idf_component_register(
SRCS
    "asic.cpp"
    "bm1366.cpp"
    "bm1368.cpp"
    "bm1370.cpp"
    "serial.cpp"
    "crc.cpp"
    "utils.cpp"
    "mining.cpp"

INCLUDE_DIRS
    "include"

REQUIRES
    "freertos"
    "driver"
    "stratum"
)


# Include the header files from "main" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main")

# Include the header files from "main/tasks" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main/tasks")