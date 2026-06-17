# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "3.5.0")

# Parse sdkconfig to determine project name based on board type
if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig")
    file(STRINGS "${CMAKE_SOURCE_DIR}/sdkconfig" SDKCONFIG_LINES)
    foreach(LINE ${SDKCONFIG_LINES})
        if(LINE MATCHES "CONFIG_BOARD_TYPE_PUPPY=y")
            set(PROJECT_BIN_NAME "rig-puppy")
        elseif(LINE MATCHES "CONFIG_BOARD_TYPE_HOVER=y")
            set(PROJECT_BIN_NAME "rig-hover")
        endif()
    endforeach()
endif()
if(NOT PROJECT_BIN_NAME)
    set(PROJECT_BIN_NAME "rig-omni")
endif()
project(${PROJECT_BIN_NAME})
