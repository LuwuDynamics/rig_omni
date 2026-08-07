# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "3.7.0")  # 默认兜底值，由版本脚本或 idf.py board-select 时覆盖

# Parse sdkconfig to determine project name based on board type
if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig")
    file(STRINGS "${CMAKE_SOURCE_DIR}/sdkconfig" SDKCONFIG_LINES)
    foreach(LINE ${SDKCONFIG_LINES})
        if(LINE MATCHES "CONFIG_BOARD_TYPE_PUPPY=y")
            set(PROJECT_BIN_NAME "rig-puppy")
            set(BOARD_DIR "puppy")
        elseif(LINE MATCHES "CONFIG_BOARD_TYPE_HOVER=y")
            set(PROJECT_BIN_NAME "rig-hover")
            set(BOARD_DIR "hover")
        elseif(LINE MATCHES "CONFIG_BOARD_TYPE_ARM=y")
            set(PROJECT_BIN_NAME "rig-arm")
            set(BOARD_DIR "arm")
        elseif(LINE MATCHES "CONFIG_BOARD_TYPE_BOT=y")
            set(PROJECT_BIN_NAME "rig-bot")
            set(BOARD_DIR "bot")
        endif()
    endforeach()
endif()
if(NOT PROJECT_BIN_NAME)
    set(PROJECT_BIN_NAME "rig-omni")
endif()

# 从板级 version.txt 读取版本号，取代全局统一版本
if(DEFINED BOARD_DIR)
    set(BOARD_VERSION_FILE "${CMAKE_SOURCE_DIR}/main/boards/${BOARD_DIR}/version.txt")
    if(EXISTS "${BOARD_VERSION_FILE}")
        file(STRINGS "${BOARD_VERSION_FILE}" BOARD_VERSION LIMIT_COUNT 1)
        if(BOARD_VERSION)
            string(STRIP "${BOARD_VERSION}" BOARD_VERSION)
            set(PROJECT_VER "${BOARD_VERSION}")
            message(STATUS "Board [${BOARD_DIR}] version: ${PROJECT_VER}")
        endif()
    endif()
endif()
project(${PROJECT_BIN_NAME})

# 自定义命令：显示当前编译的板子和版本信息
# 用法: idf.py board-info
add_custom_target(board-info
    COMMAND ${CMAKE_COMMAND} -E echo "========================================"
    COMMAND ${CMAKE_COMMAND} -E echo "  Board:      ${PROJECT_BIN_NAME}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Version:    ${PROJECT_VER}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Chip:       ${IDF_TARGET}"
    COMMAND ${CMAKE_COMMAND} -E echo "  ESP-IDF:    $ENV{IDF_PATH}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Ver File:   main/boards/${BOARD_DIR}/version.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================"
)
