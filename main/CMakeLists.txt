# Define source files
set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            "audio/demuxer/ogg_demuxer.cc"
            "audio/codecs/no_audio_codec.cc"
            "audio/codecs/box_audio_codec.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/codecs/es8374_audio_codec.cc"
            "audio/codecs/es8388_audio_codec.cc"
            "audio/codecs/es8389_audio_codec.cc"
            "audio/codecs/dummy_audio_codec.cc"
            "audio/processors/audio_debugger.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "display/lvgl_display/lvgl_display.cc"
            "display/emote_display.cc"
            "display/lvgl_display/emoji_collection.cc"
            "display/lvgl_display/lvgl_theme.cc"
            "display/lvgl_display/lvgl_font.cc"
            "display/lvgl_display/lvgl_image.cc"
            "display/lvgl_display/gif/lvgl_gif.cc"
            "display/lvgl_display/gif/gifdec.c"
            "display/cbin_font.c"
            "display/lvgl_display/jpg/image_to_jpeg.cpp"
            "display/lvgl_display/jpg/jpeg_to_image.c"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "device_state_machine.cc"
            "assets.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "display/lvgl_display" "display/lvgl_display/jpg" "audio" "audio/demuxer" "protocols")

# Add board common files
list(APPEND SOURCES
    "boards/common/board.cc"
    "boards/common/wifi_board.cc"
    "boards/common/ml307_board.cc"
    "boards/common/nt26_board.cc"
    "boards/common/dual_network_board.cc"
    "boards/common/adc_battery_monitor.cc"
    "boards/common/afsk_demod.cc"
    "boards/common/axp2101.cc"
    "boards/common/backlight.cc"
    "boards/common/button.cc"
    "boards/common/i2c_device.cc"
    "boards/common/imu.cc"
    "boards/common/knob.cc"
    "boards/common/lulu_ble.cc"
    "boards/common/power_save_timer.cc"
    "boards/common/press_to_talk_mcp_tool.cc"
    "boards/common/sleep_timer.cc"
    "boards/common/sy6970.cc"
    "boards/common/system_reset.cc"
)
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)


idf_build_get_property(build_components BUILD_COMPONENTS)
# Function to find component dynamically by pattern
function(find_component_by_pattern PATTERN COMPONENT_VAR PATH_VAR)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${PATTERN}")
            set(${COMPONENT_VAR} ${COMPONENT} PARENT_SCOPE)
            idf_component_get_property(COMPONENT_PATH ${COMPONENT} COMPONENT_DIR)
            set(${PATH_VAR} "${COMPONENT_PATH}" PARENT_SCOPE)
            break()
        endif()
    endforeach()
endfunction()

# Set default BUILTIN_TEXT_FONT and BUILTIN_ICON_FONT
set(BUILTIN_TEXT_FONT font_puhui_14_1)
set(BUILTIN_ICON_FONT font_awesome_14_1)

set(EMOTE_RESOLUTION "320_240")

# Add board files according to BOARD_TYPE
# Set default assets if the board uses partition table V2
# Board-specific embedded files (e.g., custom sounds)
set(BOARD_EMBED_FILES "")

# rig_board: shared config for RIG series boards
macro(rig_board dir name bin_name)
    set(BOARD_DIR "${dir}")
    set(BOARD_TYPE "${name}")
    set(PROJECT_BIN_NAME "${bin_name}")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_puhui_basic_20_4)
    set(EMOTE_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}")
    set(EMOTE_RESOLUTION "240_240")
    set(WAKENET_MODEL "wn9_xiaolutongxue")
    set(WAKENET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/wakenet/${WAKENET_MODEL}")
    set(BUILD_CUSTOM_ASSETS TRUE)
    file(GLOB BOARD_OGG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.ogg")
    list(APPEND BOARD_EMBED_FILES ${BOARD_OGG_FILES})
endmacro()

if(CONFIG_BOARD_TYPE_PUPPY)
    rig_board("puppy" "RIG-Puppy" "rig-puppy")
elseif(CONFIG_BOARD_TYPE_HOVER)
    rig_board("hover" "RIG-Hover" "rig-hover")
elseif(CONFIG_BOARD_TYPE_ARM)
    rig_board("arm" "RIG-Arm" "rig-arm")
elseif(CONFIG_BOARD_TYPE_BOT)
    rig_board("bot" "RIG-Bot" "rig-bot")
endif()

# 根据板型命名输出文件
set(CMAKE_PROJECT_NAME "${PROJECT_BIN_NAME}" CACHE STRING "" FORCE)

# 将板型目录加入 include path，让 common/imu.h 能引用板型专属的 xgo.h
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR})

if(MANUFACTURER)
    file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${MANUFACTURER}/${BOARD_DIR}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${MANUFACTURER}/${BOARD_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${MANUFACTURER}/${BOARD_DIR}/*.cpp
    )
else()
    file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.cpp
    )
endif()
list(APPEND SOURCES ${BOARD_SOURCES})

# Select audio processor according to Kconfig
if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio/processors/afe_audio_processor.cc")
else()
    list(APPEND SOURCES "audio/processors/no_audio_processor.cc")
endif()
if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND SOURCES "audio/wake_words/afe_wake_word.cc")
    list(APPEND SOURCES "audio/wake_words/custom_wake_word.cc")
else()
    list(APPEND SOURCES "audio/wake_words/esp_wake_word.cc")
endif()

# Auto Select Additional Sources
if (CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING)
    list(APPEND SOURCES "boards/common/blufi.cpp")
endif ()

# Language runtime (NVS-backed runtime switching)
list(APPEND SOURCES "lang_runtime.cc")
# ============================================================
# Multi-language support: embed both zh-CN and en-US OGG files
# with language prefix to avoid symbol conflicts.
# ============================================================

# Define generation paths
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")

# Build directory for language-prefixed OGG copies
set(LANG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/lang_sounds")
file(MAKE_DIRECTORY ${LANG_BUILD_DIR})

# For each language, copy OGGs to build dir with language prefix
# e.g., locales/zh-CN/welcome.ogg -> build/lang_sounds/zh_CN_welcome.ogg
# This ensures unique ESP-IDF embed symbols per language.

set(ALL_LANG_SOUNDS "")

file(GLOB ZH_CN_OGGS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/zh-CN/*.ogg)
foreach(OGG ${ZH_CN_OGGS})
    get_filename_component(FNAME ${OGG} NAME)
    set(PREFIXED "${LANG_BUILD_DIR}/zh_CN_${FNAME}")
    add_custom_command(
        OUTPUT ${PREFIXED}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OGG} ${PREFIXED}
        DEPENDS ${OGG}
        COMMENT "Copy zh-CN OGG: ${FNAME}"
    )
    list(APPEND ALL_LANG_SOUNDS ${PREFIXED})
endforeach()

file(GLOB EN_US_OGGS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/*.ogg)
foreach(OGG ${EN_US_OGGS})
    get_filename_component(FNAME ${OGG} NAME)
    set(PREFIXED "${LANG_BUILD_DIR}/en_US_${FNAME}")
    add_custom_command(
        OUTPUT ${PREFIXED}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OGG} ${PREFIXED}
        DEPENDS ${OGG}
        COMMENT "Copy en-US OGG: ${FNAME}"
    )
    list(APPEND ALL_LANG_SOUNDS ${PREFIXED})
endforeach()

file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.ogg)

# If target chip is ESP32, exclude specific files to avoid build errors
if(CONFIG_IDF_TARGET_ESP32)
    list(REMOVE_ITEM SOURCES "audio/codecs/box_audio_codec.cc"
                             "audio/codecs/es8388_audio_codec.cc"
                             "audio/codecs/es8389_audio_codec.cc"
                             "led/gpio_led.cc"
                             "display/lvgl_display/jpg/image_to_jpeg.cpp"
                             "display/lvgl_display/jpg/jpeg_to_image.c"
                             "boards/common/nt26_board.cc"
                             "boards/common/ml307_board.cc"
                             "boards/common/dual_network_board.cc"
                             )
endif()

# Include EspVideo if target is ESP32S3 or ESP32P4
if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND SOURCES "boards/common/esp_video.cc"
                        "boards/common/rndis_board.cc"
                        )
endif()

# Include Esp32Camera if target is ESP32S3
if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND SOURCES "boards/common/esp32_camera.cc")
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${ALL_LANG_SOUNDS} ${COMMON_SOUNDS} ${BOARD_EMBED_FILES}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    PRIV_REQUIRES
                        esp_pm
                        esp_psram
                        esp_netif
                        esp_driver_gpio
                        esp_driver_uart
                        esp_driver_spi
                        esp_driver_i2c
                        esp_driver_i2s
                        esp_driver_jpeg
                        esp_driver_ppa
                        esp_app_format
                        app_update
                        spi_flash
                        console
                        efuse
                        bt
                    )

# Use target_compile_definitions to define BOARD_TYPE, BOARD_NAME
# If BOARD_NAME is empty, use BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    PRIVATE BUILTIN_TEXT_FONT=${BUILTIN_TEXT_FONT} BUILTIN_ICON_FONT=${BUILTIN_ICON_FONT}
                    )

# Add generation rules
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND ${PYTHON} ${PROJECT_DIR}/tools/gen_lang.py
            --output "${LANG_HEADER}"
    DEPENDS
        ${PROJECT_DIR}/tools/gen_lang.py
    COMMENT "Generating multi-language config"
)

# Force build generation dependencies
add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER}
)

# Find ESP-SR component dynamically
find_component_by_pattern("espressif__esp-sr" ESP_SR_COMPONENT ESP_SR_COMPONENT_PATH)
if(ESP_SR_COMPONENT_PATH)
    set(ESP_SR_MODEL_PATH "${ESP_SR_COMPONENT_PATH}/model")
endif()

# Find esp_emote_assets component dynamically (provides CBin font files)
find_component_by_pattern("esp_emote_assets" EMOTE_ASSETS_COMPONENT EMOTE_ASSETS_COMPONENT_PATH)
if(EMOTE_ASSETS_COMPONENT_PATH)
    set(EMOTE_FONTS_PATH "${EMOTE_ASSETS_COMPONENT_PATH}/font")
endif()

# Function to build board custom assets (wakenet + EAF emotes)
function(build_board_custom_assets)
    set(ASSETS_BIN "${CMAKE_BINARY_DIR}/board_assets.bin")
    set(BUILD_SCRIPT "${PROJECT_DIR}/tools/spiffs_assets/build.py")
    set(EMOTE_FONTS_CBIN "${EMOTE_FONTS_PATH}")
    set(ESP_SR_WAKENET_PATH "${PROJECT_DIR}/managed_components/espressif__esp-sr/model/wakenet_model")
    
    # Copy wakenet model from board directory to ESP-SR model directory if not exists
    set(WAKENET_DST "${ESP_SR_WAKENET_PATH}/${WAKENET_MODEL}")
    if(EXISTS "${WAKENET_SRC}" AND NOT EXISTS "${WAKENET_DST}")
        message(STATUS "Copying wakenet model: ${WAKENET_SRC} -> ${WAKENET_DST}")
        file(COPY "${WAKENET_SRC}" DESTINATION "${ESP_SR_WAKENET_PATH}")
    endif()
    
    # Build arguments for board assets
    set(BUILD_ARGS
        "--text_font" "${EMOTE_FONTS_CBIN}/font_puhui_common_20_4.bin"
        "--res_path" "${EMOTE_EXTERNAL_PATH}/emoji"
        "--target_board" "${EMOTE_EXTERNAL_PATH}/${EMOTE_RESOLUTION}"
    )
    
    # Collect all EAF files as dependencies
    file(GLOB EAF_FILES "${EMOTE_EXTERNAL_PATH}/emoji/*.eaf")
    
    # Select wificonfig.eaf based on firmware region
    if(CONFIG_FIRMWARE_REGION_OVERSEAS)
        set(WIFICONFIG_SRC "${EMOTE_EXTERNAL_PATH}/emoji/wificonfig_overseas.eaf")
        message(STATUS "Overseas firmware: using overseas wificonfig.eaf")
    else()
        set(WIFICONFIG_SRC "${EMOTE_EXTERNAL_PATH}/emoji/wificonfig_domestic.eaf")
        message(STATUS "Domestic firmware: using domestic wificonfig.eaf")
    endif()
    
    # Create wakenet staging directory with all needed models
    set(WAKENET_STAGING "${CMAKE_BINARY_DIR}/wakenet_staging")
    set(WAKENET_STAGING_CMDS
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${WAKENET_STAGING}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WAKENET_STAGING}"
        # Always include custom wake word model
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${WAKENET_SRC}" "${WAKENET_STAGING}/${WAKENET_MODEL}"
    )
    # Include Kconfig-selected models (e.g. Hey Kira for overseas)
    if(CONFIG_SR_WN_WN9_HEYKIRA_TTS3)
        set(HEYKIRA_MODEL_DIR "${ESP_SR_WAKENET_PATH}/wn9_heykira_tts3")
        list(APPEND WAKENET_STAGING_CMDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${HEYKIRA_MODEL_DIR}" "${WAKENET_STAGING}/wn9_heykira_tts3"
        )
        message(STATUS "Including Hey Kira wake word model in staging")
    endif()
    list(APPEND BUILD_ARGS "--wakenet_model" "${WAKENET_STAGING}")
    
    # Create custom command to build board assets
    add_custom_command(
        OUTPUT ${ASSETS_BIN}
        COMMAND ${CMAKE_COMMAND} -E echo "Building board custom assets..."
        ${WAKENET_STAGING_CMDS}
        COMMAND ${CMAKE_COMMAND} -E copy "${WIFICONFIG_SRC}" "${EMOTE_EXTERNAL_PATH}/emoji/wificonfig.eaf"
        COMMAND ${PYTHON} ${BUILD_SCRIPT} ${BUILD_ARGS}
        COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_DIR}/tools/spiffs_assets/build/assets.bin" "${ASSETS_BIN}"
        DEPENDS
            ${BUILD_SCRIPT}
            ${EMOTE_EXTERNAL_PATH}/${EMOTE_RESOLUTION}/emote.json
            ${EMOTE_EXTERNAL_PATH}/${EMOTE_RESOLUTION}/layout.json
            ${EAF_FILES}
        WORKING_DIRECTORY ${PROJECT_DIR}/tools/spiffs_assets
        COMMENT "Building board custom assets (wakenet + EAF emotes)"
        VERBATIM
    )
    
    # Create target for board assets
    add_custom_target(board_custom_assets ALL
        DEPENDS ${ASSETS_BIN}
    )
    
    # Set the generated file path in parent scope
    set(ASSETS_LOCAL_FILE ${ASSETS_BIN} PARENT_SCOPE)
    
    message(STATUS "Board custom assets build configured: ${ASSETS_BIN}")
endfunction()


# Function to build default assets based on configuration
function(build_default_assets_bin)
    # Set output path for generated assets.bin
    set(GENERATED_ASSETS_BIN "${CMAKE_BINARY_DIR}/generated_assets.bin")
    
    # Prepare arguments for build script
    set(BUILD_ARGS
        "--sdkconfig" "${SDKCONFIG}"
        "--output" "${GENERATED_ASSETS_BIN}"
    )
    
    # Add builtin text font if defined
    if(BUILTIN_TEXT_FONT)
        list(APPEND BUILD_ARGS "--builtin_text_font" "${BUILTIN_TEXT_FONT}")
    endif()
    
    # Add default emoji collection if defined
    if(DEFAULT_EMOJI_COLLECTION)
        list(APPEND BUILD_ARGS "--emoji_collection" "${DEFAULT_EMOJI_COLLECTION}")
    endif()
    
    # Add default assets extra files if defined
    if(DEFAULT_ASSETS_EXTRA_FILES)
        list(APPEND BUILD_ARGS "--extra_files" "${DEFAULT_ASSETS_EXTRA_FILES}")
    endif()
    
    list(APPEND BUILD_ARGS "--esp_sr_model_path" "${ESP_SR_MODEL_PATH}")
    list(APPEND BUILD_ARGS "--xiaozhi_fonts_path" "${EMOTE_ASSETS_COMPONENT_PATH}")
    
    # Create custom command to build assets
    add_custom_command(
        OUTPUT ${GENERATED_ASSETS_BIN}
        COMMAND ${PYTHON} ${PROJECT_DIR}/tools/build_default_assets.py ${BUILD_ARGS}
        DEPENDS
            ${SDKCONFIG}
            ${PROJECT_DIR}/tools/build_default_assets.py
        COMMENT "Building default assets.bin based on configuration"
        VERBATIM
    )
    
    # Create target for generated assets
    add_custom_target(generated_default_assets ALL
        DEPENDS ${GENERATED_ASSETS_BIN}
    )
    
    # Set the generated file path in parent scope
    set(GENERATED_ASSETS_LOCAL_FILE ${GENERATED_ASSETS_BIN} PARENT_SCOPE)
    
    message(STATUS "Default assets build configured: ${GENERATED_ASSETS_BIN}")
endfunction()


# Function to get local assets file path (handles both URL and local file)
function(get_assets_local_file assets_source assets_local_file_var)
    # Check if it's a URL (starts with http:// or https://)
    if(assets_source MATCHES "^https?://")
        # It's a URL, download it
        get_filename_component(ASSETS_FILENAME "${assets_source}" NAME)
        set(ASSETS_LOCAL_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}")
        set(ASSETS_TEMP_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}.tmp")
        
        # Check if local file exists
        if(EXISTS ${ASSETS_LOCAL_FILE})
            message(STATUS "Assets file ${ASSETS_FILENAME} already exists, skipping download")
        else()
            message(STATUS "Downloading ${ASSETS_FILENAME}")
            
            # Clean up any existing temp file
            if(EXISTS ${ASSETS_TEMP_FILE})
                file(REMOVE ${ASSETS_TEMP_FILE})
            endif()
            
            # Download to temporary file first
            file(DOWNLOAD ${assets_source} ${ASSETS_TEMP_FILE}
                 STATUS DOWNLOAD_STATUS)
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                # Clean up temp file on failure
                if(EXISTS ${ASSETS_TEMP_FILE})
                    file(REMOVE ${ASSETS_TEMP_FILE})
                endif()
                message(FATAL_ERROR "Failed to download ${ASSETS_FILENAME} from ${assets_source}")
            endif()
            
            # Move temp file to final location (atomic operation)
            file(RENAME ${ASSETS_TEMP_FILE} ${ASSETS_LOCAL_FILE})
            message(STATUS "Successfully downloaded ${ASSETS_FILENAME}")
        endif()
    else()
        # It's a local file path
        if(IS_ABSOLUTE "${assets_source}")
            set(ASSETS_LOCAL_FILE "${assets_source}")
        else()
            set(ASSETS_LOCAL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${assets_source}")
        endif()
        
        # Check if local file exists
        if(NOT EXISTS ${ASSETS_LOCAL_FILE})
            message(FATAL_ERROR "Assets file not found: ${ASSETS_LOCAL_FILE}")
        endif()
        
        message(STATUS "Using assets file: ${ASSETS_LOCAL_FILE}")
    endif()
    
    set(${assets_local_file_var} ${ASSETS_LOCAL_FILE} PARENT_SCOPE)
endfunction()


partition_table_get_partition_info(size "--partition-name assets" "size")
partition_table_get_partition_info(offset "--partition-name assets" "offset")
if ("${size}" AND "${offset}")
    # Auto build custom assets with wakenet + EAF emotes
    if(BUILD_CUSTOM_ASSETS)
        build_board_custom_assets()
        # Use the generated assets.bin from build directory
        esptool_py_flash_to_partition(flash "assets" "${ASSETS_LOCAL_FILE}")
        add_dependencies(flash board_custom_assets)
        message(STATUS "Board custom assets flash configured: ${ASSETS_LOCAL_FILE} -> assets partition")
    # Flash assets based on configuration
    elseif(CONFIG_FLASH_DEFAULT_ASSETS)
        # Build default assets based on configuration
        build_default_assets_bin()
        esptool_py_flash_to_partition(flash "assets" "${GENERATED_ASSETS_LOCAL_FILE}")
        message(STATUS "Generated default assets flash configured: ${GENERATED_ASSETS_LOCAL_FILE} -> assets partition")
    elseif(CONFIG_FLASH_CUSTOM_ASSETS)
        # Flash custom assets
        get_assets_local_file("${CONFIG_CUSTOM_ASSETS_FILE}" ASSETS_LOCAL_FILE)
        esptool_py_flash_to_partition(flash "assets" "${ASSETS_LOCAL_FILE}")
        message(STATUS "Custom assets flash configured: ${ASSETS_LOCAL_FILE} -> assets partition")
    elseif(CONFIG_FLASH_EXPRESSION_ASSETS)
        set(ASSETS_NAME "expression_assets")
        set(ASSETS_PARTITION "assets")
        set(ASSETS_FILE "${CMAKE_BINARY_DIR}/${ASSETS_NAME}.bin")

        if(EMOTE_EXTERNAL_PATH)
            build_speaker_assets_bin("${ASSETS_PARTITION}" ${EMOTE_RESOLUTION} ${ASSETS_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH} ${EMOTE_EXTERNAL_PATH})
        else()
            build_speaker_assets_bin("${ASSETS_PARTITION}" ${EMOTE_RESOLUTION} ${ASSETS_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH})
        endif()
        message(STATUS "Generated emote assets: ${ASSETS_FILE} -> ${ASSETS_PARTITION} partition")
        esptool_py_flash_to_partition(flash "${ASSETS_PARTITION}" "${ASSETS_FILE}")
    elseif(CONFIG_FLASH_NONE_ASSETS)
        message(STATUS "Assets flashing disabled (FLASH_NONE_ASSETS)")
    endif()
else()
    message(STATUS "Assets partition not found, using v1 partition table")
endif()
