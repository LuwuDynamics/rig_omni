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
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)
    set(BOARD_TYPE "bread-compact-wifi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ML307)
    set(BOARD_TYPE "bread-compact-ml307")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_NT26)
    set(BOARD_TYPE "bread-compact-nt26")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ESP32)
    set(BOARD_TYPE "bread-compact-esp32")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_ESP32_LCD)
    set(BOARD_TYPE "bread-compact-esp32-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_DF_K10)
    set(BOARD_TYPE "df-k10")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_DF_S3_AI_CAM)
    set(BOARD_TYPE "df-s3-ai-cam")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_3)
    set(BOARD_TYPE "esp-box-3")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
    set(EMOTE_RESOLUTION "320_240")
elseif(CONFIG_BOARD_TYPE_ESP_BOX)
    set(BOARD_TYPE "esp-box")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
    set(EMOTE_RESOLUTION "320_240")
elseif(CONFIG_BOARD_TYPE_ESP_BOX_LITE)
    set(BOARD_TYPE "esp-box-lite")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_KEVIN_BOX_2)
    set(BOARD_TYPE "kevin-box-2")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_KEVIN_C3)
    set(BOARD_TYPE "kevin-c3")
elseif(CONFIG_BOARD_TYPE_KEVIN_SP_V3_DEV)
    set(BOARD_TYPE "kevin-sp-v3-dev")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_KEVIN_SP_V4_DEV)
    set(BOARD_TYPE "kevin-sp-v4-dev")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_KEVIN_YUYING_313LCD)
    set(BOARD_TYPE "kevin-yuying-313lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV_S3)
    set(BOARD_TYPE "lichuang-dev")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_LICHUANG_DEV_C3)
    set(BOARD_TYPE "lichuang-c3-dev")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_EDA_TV_PRO)
    set(MANUFACTURER "lceda-course-examples")
    set(BOARD_TYPE "eda-tv-pro")    
elseif(CONFIG_BOARD_TYPE_EDA_ROBOT_PRO)
    set(MANUFACTURER "lceda-course-examples")
    set(BOARD_TYPE "eda-robot-pro")
    set(BUILTIN_TEXT_FONT font_puhui_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
elseif(CONFIG_BOARD_TYPE_EDA_SUPER_BEAR)
    set(MANUFACTURER "lceda-course-examples")
    set(BOARD_TYPE "eda-super-bear")
elseif(CONFIG_BOARD_TYPE_MAGICLICK_S3_2P4)
    set(BOARD_TYPE "magiclick-2p4")
    set(BUILTIN_TEXT_FONT font_noto_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_64)
elseif(CONFIG_BOARD_TYPE_MAGICLICK_S3_2P5)
    set(BOARD_TYPE "magiclick-2p5")
    set(BUILTIN_TEXT_FONT font_noto_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_64)
elseif(CONFIG_BOARD_TYPE_MAGICLICK_C3)
    set(BOARD_TYPE "magiclick-c3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_MAGICLICK_C3_V2)
    set(BOARD_TYPE "magiclick-c3-v2")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_M5STACK_CORE_S3)
    set(BOARD_TYPE "m5stack-core-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_M5STACK_CORE_TAB5)
    set(BOARD_TYPE "m5stack-tab5")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_M5STACK_ATOM_S3_ECHO_BASE)
    set(BOARD_TYPE "atoms3-echo-base")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_M5STACK_ATOM_S3R_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-echo-base")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_M5STACK_ATOM_S3R_CAM_M12_ECHO_BASE)
    set(BOARD_TYPE "atoms3r-cam-m12-echo-base")
elseif(CONFIG_BOARD_TYPE_M5STACK_ATOM_ECHOS3R)
    set(BOARD_TYPE "atom-echos3r")
elseif(CONFIG_BOARD_TYPE_M5STACK_CARDPUTER_ADV)
    set(BOARD_TYPE "m5stack-cardputer-adv")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_M5STACK_ATOM_MATRIX_ECHO_BASE)
    set(BOARD_TYPE "atommatrix-echo-base")
elseif(CONFIG_BOARD_TYPE_XMINI_C3_V3)
    set(BOARD_TYPE "xmini-c3-v3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_XMINI_C3_4G)
    set(BOARD_TYPE "xmini-c3-4g")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_XMINI_C3)
    set(BOARD_TYPE "xmini-c3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_ESP_KORVO2_V3)
    set(BOARD_TYPE "esp32s3-korvo2-v3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ESP_KORVO2_V3_RNDIS)
    set(BOARD_TYPE "esp32s3-korvo2-v3-rndis")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ESP_SPARKBOT)
    set(BOARD_TYPE "esp-sparkbot")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ESP_SPOT_S3)
    set(BOARD_TYPE "esp-spot")
elseif(CONFIG_BOARD_TYPE_ESP_SPOT_C5)
    set(BOARD_TYPE "esp-spot")
elseif(CONFIG_BOARD_TYPE_ESP_HI)
    set(BOARD_TYPE "esp-hi")
    # Set ESP_HI emoji directory for DEFAULT_ASSETS_EXTRA_FILES
    set(DEFAULT_ASSETS_EXTRA_FILES "${CMAKE_BINARY_DIR}/emoji")
elseif(CONFIG_BOARD_TYPE_ECHOEAR)
    set(BOARD_TYPE "echoear")
    set(BUILTIN_TEXT_FONT font_puhui_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
    set(EMOTE_RESOLUTION "360_360")
    # set(EMOTE_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/boards/echoear/assets")
elseif(CONFIG_BOARD_TYPE_ESP_SENSAIRSHUTTLE)
    set(BOARD_TYPE "esp-sensairshuttle")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_AUDIO_BOARD)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-audio-board")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.8")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_AMOLED_1_8)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-touch-amoled-1.8")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_06)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-amoled-2.06")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_AMOLED_2_06)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-touch-amoled-2.06")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_4B)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-4b")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3C)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-4.3c")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_75)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.75")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_75C)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.75")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_83)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.83")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_85C)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85c")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_85)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.85")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-1.46")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_3_5)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-3.5")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_3_5B)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-3.5b")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_ePaper_1_54_v1)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-epaper-1.54")
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_ePaper_1_54_v2)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-epaper-1.54")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_RLCD_4_2)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-rlcd-4.2")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_3_49)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-lcd-3.49")
    set(LVGL_TEXT_FONT font_puhui_basic_30_4)
    set(LVGL_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_LCD_1_69)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-lcd-1.69")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_LCD_1_83)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-touch-lcd-1.83")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_AMOLED_1_43)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-touch-amoled-1.43")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_C6_TOUCH_AMOLED_1_32)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-c6-touch-amoled-1.32")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_32)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-s3-touch-amoled-1.32")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_NANO)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-nano")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_4B)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_7B)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_3_4C)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_4C)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_7)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_8)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_10_1)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-wifi6-touch-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ESP_P4_FUNCTION_EV_BOARD)
    set(BOARD_TYPE "esp-p4-function-ev-board")
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD)
    set(BOARD_TYPE "bread-compact-wifi-lcd")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_TUDOUZI)
    set(BOARD_TYPE "tudouzi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CIRCLE_S3)
    set(BOARD_TYPE "lilygo-t-circle-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_0_V1_1)
    set(BOARD_TYPE "lilygo-t-cameraplus-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_2)
    set(BOARD_TYPE "lilygo-t-cameraplus-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_S3_PRO_MVSRLORA)
    set(BOARD_TYPE "lilygo-t-display-s3-pro-mvsrlora")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_S3_PRO_MVSRLORA_NO_BATTERY)
    set(BOARD_TYPE "lilygo-t-display-s3-pro-mvsrlora")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_P4)
    set(BOARD_TYPE "lilygo-t-display-p4")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_MOVECALL_MOJI_ESP32S3)
    set(BOARD_TYPE "movecall-moji-esp32s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_MOVECALL_MOJI2_ESP32C5)
    set(BOARD_TYPE "movecall-moji2-esp32c5")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_MOVECALL_CUICAN_ESP32S3)
    set(BOARD_TYPE "movecall-cuican-esp32s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3)
    set(BOARD_TYPE "atk-dnesp32s3")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX)
    set(BOARD_TYPE "atk-dnesp32s3-box")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX0)
    set(BOARD_TYPE "atk-dnesp32s3-box0")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX2_WIFI)
    set(BOARD_TYPE "atk-dnesp32s3-box2-wifi")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3_BOX2_4G)
    set(BOARD_TYPE "atk-dnesp32s3-box2-4g")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3M_WIFI)
    set(BOARD_TYPE "atk-dnesp32s3m-wifi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_ATK_DNESP32S3M_4G)
    set(BOARD_TYPE "atk-dnesp32s3m-4g")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_DU_CHATX)
    set(BOARD_TYPE "du-chatx")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_TAIJI_PI_S3)
    set(BOARD_TYPE "taiji-pi-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_0_85TFT_WIFI)
    set(BOARD_TYPE "xingzhi-cube-0.85tft-wifi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_0_85TFT_ML307)
    set(BOARD_TYPE "xingzhi-cube-0.85tft-ml307")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_0_96OLED_WIFI)
    set(BOARD_TYPE "xingzhi-cube-0.96oled-wifi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_0_96OLED_ML307)
    set(BOARD_TYPE "xingzhi-cube-0.96oled-ml307")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_1_54TFT_WIFI)
    set(BOARD_TYPE "xingzhi-cube-1.54tft-wifi")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_XINGZHI_CUBE_1_54TFT_ML307)
    set(BOARD_TYPE "xingzhi-cube-1.54tft-ml307")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_XINGZHI_METAL_1_54_WIFI)
    set(BOARD_TYPE "xingzhi-metal-1.54-wifi")
    set(BUILTIN_TEXT_FONT font_noto_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)  
elseif(CONFIG_BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER)
    set(BOARD_TYPE "sensecap-watcher")
    set(BUILTIN_TEXT_FONT font_noto_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-emoji_128)
elseif(CONFIG_BOARD_TYPE_DOIT_S3_AIBOX)
    set(BOARD_TYPE "doit-s3-aibox")
elseif(CONFIG_BOARD_TYPE_MIXGO_NOVA)
    set(BOARD_TYPE "mixgo-nova")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_GENJUTECH_S3_1_54TFT)
    set(BOARD_TYPE "genjutech-s3-1.54tft")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_CGC)
    set(BOARD_TYPE "esp32-cgc")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_CGC_144)
    set(BOARD_TYPE "esp32-cgc-144")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_ESP_S3_LCD_EV_Board)
    set(BOARD_TYPE "esp-s3-lcd-ev-board")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ESP_S3_LCD_EV_Board_2)
    set(BOARD_TYPE "esp-s3-lcd-ev-board-2")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_1_54TFT_WIFI)
    set(BOARD_TYPE "zhengchen-1.54tft-wifi")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_MINSI_K08_DUAL)
    set(BOARD_TYPE "minsi-k08-dual")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307)
    set(BOARD_TYPE "zhengchen-1.54tft-ml307")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_CAM)
    set(BOARD_TYPE "zhengchen-cam")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_ZHENGCHEN_CAM_ML307)
    set(BOARD_TYPE "zhengchen-cam-ml307")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)    
elseif(CONFIG_BOARD_TYPE_SPOTPEAR_ESP32_S3_1_54_MUMA)
    set(BOARD_TYPE "sp-esp32-s3-1.54-muma")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_SPOTPEAR_ESP32_S3_1_28_BOX)
    set(BOARD_TYPE "sp-esp32-s3-1.28-box")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_OTTO_ROBOT)
    set(BOARD_TYPE "otto-robot")
    set(BUILTIN_TEXT_FONT font_puhui_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION otto-gif)
elseif(CONFIG_BOARD_TYPE_ELECTRON_BOT)
    set(BOARD_TYPE "electron-bot")
    set(BUILTIN_TEXT_FONT font_puhui_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION otto-gif)
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_CAM)
    set(BOARD_TYPE "bread-compact-wifi-s3cam")
    set(BUILTIN_TEXT_FONT font_puhui_basic_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_JIUCHUAN)
    set(BOARD_TYPE "jiuchuan-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LABPLUS_MPYTHON_V3)
    set(BOARD_TYPE "labplus-mpython-v3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_LABPLUS_LEDONG_V2)
    set(BOARD_TYPE "labplus-ledong-v2")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_SURFER_C3_1_14TFT)
    set(BOARD_TYPE "surfer-c3-1.14tft")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_YUNLIAO_S3)
    set(BOARD_TYPE "yunliao-s3")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_WIRELESS_TAG_WTP4C5MP07S)
    set(BOARD_TYPE "wireless-tag-wtp4c5mp07s")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
elseif(CONFIG_BOARD_TYPE_AIPI_LITE)
    set(BOARD_TYPE "aipi-lite")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
    set(DEFAULT_EMOJI_COLLECTION twemoji_32)
elseif(CONFIG_BOARD_TYPE_HU_087)
    set(BOARD_TYPE "hu-087")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
elseif(CONFIG_BOARD_TYPE_PUPPY)
    set(BOARD_DIR "puppy")
    set(BOARD_TYPE "RIG-Puppy")
    set(PROJECT_BIN_NAME "rig-puppy")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_puhui_basic_20_4)
    # 使用自定义 EAF 表情
    set(EMOTE_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}")
    set(EMOTE_RESOLUTION "240_240")
    # Puppy 专用唤醒词模型配置
    set(WAKENET_MODEL "wn9_xiaolutongxue")
    set(WAKENET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/wakenet/${WAKENET_MODEL}")
    set(BUILD_CUSTOM_ASSETS TRUE)
elseif(CONFIG_BOARD_TYPE_HOVER)
    set(BOARD_DIR "hover")
    set(BOARD_TYPE "RIG-Hover")
    set(PROJECT_BIN_NAME "rig-hover")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)
    set(BUILTIN_ICON_FONT font_puhui_basic_20_4)
    # 使用自定义 EAF 表情
    set(EMOTE_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}")
    set(EMOTE_RESOLUTION "240_240")
    # Hover 专用唤醒词模型配置
    set(WAKENET_MODEL "wn9_xiaolutongxue")
    set(WAKENET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/wakenet/${WAKENET_MODEL}")
    set(BUILD_CUSTOM_ASSETS TRUE)
endif()

# 根据板型命名输出文件
set(CMAKE_PROJECT_NAME "${PROJECT_BIN_NAME}" CACHE STRING "" FORCE)

# 将板型目录加入 include path，让 common/imu.h 能引用板型专属的 xgo.h
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR})

if(MANUFACTURER)
    file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${MANUFACTURER}/${BOARD_DIR}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${MANUFACTURER}/${BOARD_DIR}/*.c
    )
else()
    file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.c
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
# Select language directory according to Kconfig
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
endif()

# Define generation path
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")

# Collect current language audio files
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)

# If not en-US, collect en-US audio files as fallback for missing files
if(NOT LANG_DIR STREQUAL "en-US")
    file(GLOB EN_US_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/*.ogg)
    
    # Extract filenames (without path) from current language
    set(EXISTING_NAMES "")
    foreach(SOUND_FILE ${LANG_SOUNDS})
        get_filename_component(FILENAME ${SOUND_FILE} NAME)
        list(APPEND EXISTING_NAMES ${FILENAME})
    endforeach()
    
    # Only add en-US audio files that are missing in current language
    foreach(EN_SOUND ${EN_US_SOUNDS})
        get_filename_component(FILENAME ${EN_SOUND} NAME)
        if(NOT ${FILENAME} IN_LIST EXISTING_NAMES)
            list(APPEND LANG_SOUNDS ${EN_SOUND})
            message(STATUS "Using en-US fallback for missing audio: ${FILENAME}")
        endif()
    endforeach()
endif()

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
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
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
    COMMAND python ${PROJECT_DIR}/tools/gen_lang.py
            --language "${LANG_DIR}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/tools/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
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

if(CONFIG_BOARD_TYPE_ESP_HI)
set(URL "https://github.com/espressif2022/image_player/raw/main/test_apps/test_8bit")
set(EMOJI_DIR "${CMAKE_BINARY_DIR}/emoji")
file(MAKE_DIRECTORY ${EMOJI_DIR})

# List all files to download
set(FILES_TO_DOWNLOAD "")
list(APPEND FILES_TO_DOWNLOAD "Anger_enter.aaf" "Anger_loop.aaf" "Anger_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "happy_enter.aaf" "happy_loop.aaf" "happ_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "sad_enter.aaf" "sad_loop.aaf" "sad_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "scorn_enter.aaf" "scorn_loop.aaf" "scorn_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "left_enter.aaf" "left_loop.aaf" "left_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "right_enter.aaf" "right_loop.aaf" "right_return.aaf")
list(APPEND FILES_TO_DOWNLOAD "asking.aaf" "blink_once.aaf" "blink_quick.aaf")
list(APPEND FILES_TO_DOWNLOAD "connecting.aaf" "panic_enter.aaf" "panic_loop.aaf")
list(APPEND FILES_TO_DOWNLOAD "panic_return.aaf" "wake.aaf")

foreach(FILENAME IN LISTS FILES_TO_DOWNLOAD)
    set(REMOTE_FILE "${URL}/${FILENAME}")
    set(LOCAL_FILE "${EMOJI_DIR}/${FILENAME}")
    
    # Check if local file exists
    if(EXISTS ${LOCAL_FILE})
        message(STATUS "File ${FILENAME} already exists, skipping download")
    else()
        message(STATUS "Downloading ${FILENAME}")
        file(DOWNLOAD ${REMOTE_FILE} ${LOCAL_FILE}
             STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ${FILENAME} from ${URL}")
        endif()
    endif()
endforeach()

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
        "--wakenet_model" "${WAKENET_DST}"
        "--text_font" "${EMOTE_FONTS_CBIN}/font_puhui_common_20_4.bin"
        "--res_path" "${EMOTE_EXTERNAL_PATH}/emoji"
        "--target_board" "${EMOTE_EXTERNAL_PATH}/${EMOTE_RESOLUTION}"
    )
    
    # Collect all EAF files as dependencies
    file(GLOB EAF_FILES "${EMOTE_EXTERNAL_PATH}/emoji/*.eaf")
    
    # Create custom command to build board assets
    add_custom_command(
        OUTPUT ${ASSETS_BIN}
        COMMAND ${CMAKE_COMMAND} -E echo "Building board custom assets..."
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${WAKENET_SRC}" "${WAKENET_DST}"
        COMMAND python ${BUILD_SCRIPT} ${BUILD_ARGS}
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
        COMMAND python ${PROJECT_DIR}/tools/build_default_assets.py ${BUILD_ARGS}
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
