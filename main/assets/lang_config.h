// Auto-generated runtime language config
// Supports: en-US, zh-CN
#pragma once

#include <string_view>

namespace Lang {

    enum class Code : uint8_t { en_US, zh_CN };

    void Initialize();            // 从 NVS 读取语言设置
    void SetLanguage(Code lang);  // 运行时切换语言 + 写入 NVS
    Code GetLanguage();           // 获取当前语言
    const char* CodeStr();        // 返回如 "zh-CN" / "en-US"

    // 字符串资源 — 运行时按语言分发
    namespace Strings {
        inline const char* ACCESS_VIA_BROWSER() {
            switch (GetLanguage()) {
                case Code::en_US: return " Config URL: ";
                case Code::zh_CN: return "，浏览器访问 ";
            }
            return " Config URL: ";
        }
        inline const char* ACTIVATION() {
            switch (GetLanguage()) {
                case Code::en_US: return "Activation";
                case Code::zh_CN: return "激活设备";
            }
            return "Activation";
        }
        inline const char* BATTERY_CHARGING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Charging";
                case Code::zh_CN: return "正在充电";
            }
            return "Charging";
        }
        inline const char* BATTERY_FULL() {
            switch (GetLanguage()) {
                case Code::en_US: return "Battery full";
                case Code::zh_CN: return "电量已满";
            }
            return "Battery full";
        }
        inline const char* BATTERY_LOW() {
            switch (GetLanguage()) {
                case Code::en_US: return "Low battery";
                case Code::zh_CN: return "电量不足";
            }
            return "Low battery";
        }
        inline const char* BATTERY_NEED_CHARGE() {
            switch (GetLanguage()) {
                case Code::en_US: return "Low battery, please charge";
                case Code::zh_CN: return "电量低，请充电";
            }
            return "Low battery, please charge";
        }
        inline const char* CHECKING_NEW_VERSION() {
            switch (GetLanguage()) {
                case Code::en_US: return "Checking for new version...";
                case Code::zh_CN: return "检查新版本...";
            }
            return "Checking for new version...";
        }
        inline const char* CHECK_NEW_VERSION_FAILED() {
            switch (GetLanguage()) {
                case Code::en_US: return "Check for new version failed, will retry in %d seconds: %s";
                case Code::zh_CN: return "检查新版本失败，将在 %d 秒后重试：%s";
            }
            return "Check for new version failed, will retry in %d seconds: %s";
        }
        inline const char* CONNECTED_TO() {
            switch (GetLanguage()) {
                case Code::en_US: return "Connected to ";
                case Code::zh_CN: return "已连接 ";
            }
            return "Connected to ";
        }
        inline const char* CONNECTING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Connecting...";
                case Code::zh_CN: return "连接中...";
            }
            return "Connecting...";
        }
        inline const char* CONNECTION_SUCCESSFUL() {
            switch (GetLanguage()) {
                case Code::en_US: return "Connection Successful";
                case Code::zh_CN: return "连接成功";
            }
            return "Connection Successful";
        }
        inline const char* CONNECT_TO() {
            switch (GetLanguage()) {
                case Code::en_US: return "Connect to ";
                case Code::zh_CN: return "连接 ";
            }
            return "Connect to ";
        }
        inline const char* CONNECT_TO_HOTSPOT() {
            switch (GetLanguage()) {
                case Code::en_US: return "Hotspot: ";
                case Code::zh_CN: return "手机连接热点 ";
            }
            return "Hotspot: ";
        }
        inline const char* DETECTING_MODULE() {
            switch (GetLanguage()) {
                case Code::en_US: return "Detecting module...";
                case Code::zh_CN: return "检测模组...";
            }
            return "Detecting module...";
        }
        inline const char* DOWNLOAD_ASSETS_FAILED() {
            switch (GetLanguage()) {
                case Code::en_US: return "Failed to download assets";
                case Code::zh_CN: return "下载资源失败";
            }
            return "Failed to download assets";
        }
        inline const char* ENTERING_WIFI_CONFIG_MODE() {
            switch (GetLanguage()) {
                case Code::en_US: return "Entering Wi-Fi configuration mode...";
                case Code::zh_CN: return "进入配网模式...";
            }
            return "Entering Wi-Fi configuration mode...";
        }
        inline const char* ERROR() {
            switch (GetLanguage()) {
                case Code::en_US: return "Error";
                case Code::zh_CN: return "错误";
            }
            return "Error";
        }
        inline const char* FLIGHT_MODE_OFF() {
            switch (GetLanguage()) {
                case Code::en_US: return "Flight mode is off";
                case Code::zh_CN: return "飞行模式已关闭";
            }
            return "Flight mode is off";
        }
        inline const char* FLIGHT_MODE_ON() {
            switch (GetLanguage()) {
                case Code::en_US: return "Flight mode is on";
                case Code::zh_CN: return "飞行模式已开启";
            }
            return "Flight mode is on";
        }
        inline const char* FOUND_NEW_ASSETS() {
            switch (GetLanguage()) {
                case Code::en_US: return "Found new assets: %s";
                case Code::zh_CN: return "发现新资源: %s";
            }
            return "Found new assets: %s";
        }
        inline const char* HELLO_MY_FRIEND() {
            switch (GetLanguage()) {
                case Code::en_US: return "Hello, my friend!";
                case Code::zh_CN: return "你好，我的朋友！";
            }
            return "Hello, my friend!";
        }
        inline const char* INFO() {
            switch (GetLanguage()) {
                case Code::en_US: return "Information";
                case Code::zh_CN: return "信息";
            }
            return "Information";
        }
        inline const char* INITIALIZING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Initializing...";
                case Code::zh_CN: return "正在初始化...";
            }
            return "Initializing...";
        }
        inline const char* LISTENING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Listening...";
                case Code::zh_CN: return "聆听中...";
            }
            return "Listening...";
        }
        inline const char* LOADING_ASSETS() {
            switch (GetLanguage()) {
                case Code::en_US: return "Loading assets...";
                case Code::zh_CN: return "加载资源...";
            }
            return "Loading assets...";
        }
        inline const char* LOADING_PROTOCOL() {
            switch (GetLanguage()) {
                case Code::en_US: return "Logging in...";
                case Code::zh_CN: return "登录服务器...";
            }
            return "Logging in...";
        }
        inline const char* MAX_VOLUME() {
            switch (GetLanguage()) {
                case Code::en_US: return "Max volume";
                case Code::zh_CN: return "最大音量";
            }
            return "Max volume";
        }
        inline const char* MODEM_INIT_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US: return "Modem initialization failed";
                case Code::zh_CN: return "模组初始化失败";
            }
            return "Modem initialization failed";
        }
        inline const char* MUTED() {
            switch (GetLanguage()) {
                case Code::en_US: return "Muted";
                case Code::zh_CN: return "已静音";
            }
            return "Muted";
        }
        inline const char* NEW_VERSION() {
            switch (GetLanguage()) {
                case Code::en_US: return "New version ";
                case Code::zh_CN: return "新版本 ";
            }
            return "New version ";
        }
        inline const char* OTA_UPGRADE() {
            switch (GetLanguage()) {
                case Code::en_US: return "OTA Upgrade";
                case Code::zh_CN: return "OTA 升级";
            }
            return "OTA Upgrade";
        }
        inline const char* PIN_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US: return "Please insert SIM card";
                case Code::zh_CN: return "请插入 SIM 卡";
            }
            return "Please insert SIM card";
        }
        inline const char* PLEASE_WAIT() {
            switch (GetLanguage()) {
                case Code::en_US: return "Please wait...";
                case Code::zh_CN: return "请稍候...";
            }
            return "Please wait...";
        }
        inline const char* REGISTERING_NETWORK() {
            switch (GetLanguage()) {
                case Code::en_US: return "Waiting for network...";
                case Code::zh_CN: return "等待网络...";
            }
            return "Waiting for network...";
        }
        inline const char* REG_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US: return "Unable to access network, please check SIM card status";
                case Code::zh_CN: return "无法接入网络，请检查流量卡状态";
            }
            return "Unable to access network, please check SIM card status";
        }
        inline const char* RTC_MODE_OFF() {
            switch (GetLanguage()) {
                case Code::en_US: return "AEC Off";
                case Code::zh_CN: return "AEC 关闭";
            }
            return "AEC Off";
        }
        inline const char* RTC_MODE_ON() {
            switch (GetLanguage()) {
                case Code::en_US: return "AEC On";
                case Code::zh_CN: return "AEC 开启";
            }
            return "AEC On";
        }
        inline const char* SCANNING_WIFI() {
            switch (GetLanguage()) {
                case Code::en_US: return "Scanning Wi-Fi...";
                case Code::zh_CN: return "扫描 Wi-Fi...";
            }
            return "Scanning Wi-Fi...";
        }
        inline const char* SERVER_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US: return "Sending failed, please check the network";
                case Code::zh_CN: return "发送失败，请检查网络";
            }
            return "Sending failed, please check the network";
        }
        inline const char* SERVER_NOT_CONNECTED() {
            switch (GetLanguage()) {
                case Code::en_US: return "Unable to connect to service, please try again later";
                case Code::zh_CN: return "无法连接服务，请稍后再试";
            }
            return "Unable to connect to service, please try again later";
        }
        inline const char* SERVER_NOT_FOUND() {
            switch (GetLanguage()) {
                case Code::en_US: return "Looking for available service";
                case Code::zh_CN: return "正在寻找可用服务";
            }
            return "Looking for available service";
        }
        inline const char* SERVER_TIMEOUT() {
            switch (GetLanguage()) {
                case Code::en_US: return "Waiting for response timeout";
                case Code::zh_CN: return "等待响应超时";
            }
            return "Waiting for response timeout";
        }
        inline const char* SPEAKING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Speaking...";
                case Code::zh_CN: return "说话中...";
            }
            return "Speaking...";
        }
        inline const char* STANDBY() {
            switch (GetLanguage()) {
                case Code::en_US: return "Standby";
                case Code::zh_CN: return "待命";
            }
            return "Standby";
        }
        inline const char* SWITCH_TO_4G_NETWORK() {
            switch (GetLanguage()) {
                case Code::en_US: return "Switching to 4G...";
                case Code::zh_CN: return "切换到 4G...";
            }
            return "Switching to 4G...";
        }
        inline const char* SWITCH_TO_WIFI_NETWORK() {
            switch (GetLanguage()) {
                case Code::en_US: return "Switching to Wi-Fi...";
                case Code::zh_CN: return "切换到 Wi-Fi...";
            }
            return "Switching to Wi-Fi...";
        }
        inline const char* THINKING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Thinking...";
                case Code::zh_CN: return "思考中...";
            }
            return "Thinking...";
        }
        inline const char* UPGRADE_FAILED() {
            switch (GetLanguage()) {
                case Code::en_US: return "Upgrade failed";
                case Code::zh_CN: return "升级失败";
            }
            return "Upgrade failed";
        }
        inline const char* UPGRADING() {
            switch (GetLanguage()) {
                case Code::en_US: return "System is upgrading...";
                case Code::zh_CN: return "正在升级系统...";
            }
            return "System is upgrading...";
        }
        inline const char* VERSION() {
            switch (GetLanguage()) {
                case Code::en_US: return "Ver ";
                case Code::zh_CN: return "版本 ";
            }
            return "Ver ";
        }
        inline const char* VOLUME() {
            switch (GetLanguage()) {
                case Code::en_US: return "Volume ";
                case Code::zh_CN: return "音量 ";
            }
            return "Volume ";
        }
        inline const char* WARNING() {
            switch (GetLanguage()) {
                case Code::en_US: return "Warning";
                case Code::zh_CN: return "警告";
            }
            return "Warning";
        }
        inline const char* WIFI_CONFIG_MODE() {
            switch (GetLanguage()) {
                case Code::en_US: return "Wi-Fi Configuration Mode";
                case Code::zh_CN: return "配网模式";
            }
            return "Wi-Fi Configuration Mode";
        }
    }

    // 音效资源 — 运行时按语言分发
    namespace Sounds {

        // Sound: activation
        extern const char ogg_zh_cn_activation_start[] asm("_binary_zh_CN_activation_ogg_start");
        extern const char ogg_zh_cn_activation_end[]   asm("_binary_zh_CN_activation_ogg_end");
        extern const char ogg_en_us_activation_start[] asm("_binary_en_US_activation_ogg_start");
        extern const char ogg_en_us_activation_end[]   asm("_binary_en_US_activation_ogg_end");
        inline std::string_view OGG_ACTIVATION() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_activation_start), static_cast<size_t>(ogg_en_us_activation_end - ogg_en_us_activation_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_activation_start), static_cast<size_t>(ogg_zh_cn_activation_end - ogg_zh_cn_activation_start)};
            }
            return {};
        }

        // Sound: activation_error
        extern const char ogg_zh_cn_activation_error_start[] asm("_binary_zh_CN_activation_error_ogg_start");
        extern const char ogg_zh_cn_activation_error_end[]   asm("_binary_zh_CN_activation_error_ogg_end");
        extern const char ogg_en_us_activation_error_start[] asm("_binary_en_US_activation_error_ogg_start");
        extern const char ogg_en_us_activation_error_end[]   asm("_binary_en_US_activation_error_ogg_end");
        inline std::string_view OGG_ACTIVATION_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_activation_error_start), static_cast<size_t>(ogg_en_us_activation_error_end - ogg_en_us_activation_error_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_activation_error_start), static_cast<size_t>(ogg_zh_cn_activation_error_end - ogg_zh_cn_activation_error_start)};
            }
            return {};
        }

        // Sound: calibration_enter
        extern const char ogg_zh_cn_calibration_enter_start[] asm("_binary_zh_CN_calibration_enter_ogg_start");
        extern const char ogg_zh_cn_calibration_enter_end[]   asm("_binary_zh_CN_calibration_enter_ogg_end");
        extern const char ogg_en_us_calibration_enter_start[] asm("_binary_en_US_calibration_enter_ogg_start");
        extern const char ogg_en_us_calibration_enter_end[]   asm("_binary_en_US_calibration_enter_ogg_end");
        inline std::string_view OGG_CALIBRATION_ENTER() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_calibration_enter_start), static_cast<size_t>(ogg_en_us_calibration_enter_end - ogg_en_us_calibration_enter_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_calibration_enter_start), static_cast<size_t>(ogg_zh_cn_calibration_enter_end - ogg_zh_cn_calibration_enter_start)};
            }
            return {};
        }

        // Sound: calibration_exit
        extern const char ogg_zh_cn_calibration_exit_start[] asm("_binary_zh_CN_calibration_exit_ogg_start");
        extern const char ogg_zh_cn_calibration_exit_end[]   asm("_binary_zh_CN_calibration_exit_ogg_end");
        extern const char ogg_en_us_calibration_exit_start[] asm("_binary_en_US_calibration_exit_ogg_start");
        extern const char ogg_en_us_calibration_exit_end[]   asm("_binary_en_US_calibration_exit_ogg_end");
        inline std::string_view OGG_CALIBRATION_EXIT() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_calibration_exit_start), static_cast<size_t>(ogg_en_us_calibration_exit_end - ogg_en_us_calibration_exit_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_calibration_exit_start), static_cast<size_t>(ogg_zh_cn_calibration_exit_end - ogg_zh_cn_calibration_exit_start)};
            }
            return {};
        }

        // Sound: close_aec
        extern const char ogg_zh_cn_close_aec_start[] asm("_binary_zh_CN_close_aec_ogg_start");
        extern const char ogg_zh_cn_close_aec_end[]   asm("_binary_zh_CN_close_aec_ogg_end");
        extern const char ogg_en_us_close_aec_start[] asm("_binary_en_US_close_aec_ogg_start");
        extern const char ogg_en_us_close_aec_end[]   asm("_binary_en_US_close_aec_ogg_end");
        inline std::string_view OGG_CLOSE_AEC() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_close_aec_start), static_cast<size_t>(ogg_en_us_close_aec_end - ogg_en_us_close_aec_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_close_aec_start), static_cast<size_t>(ogg_zh_cn_close_aec_end - ogg_zh_cn_close_aec_start)};
            }
            return {};
        }

        // Sound: connect_error
        extern const char ogg_zh_cn_connect_error_start[] asm("_binary_zh_CN_connect_error_ogg_start");
        extern const char ogg_zh_cn_connect_error_end[]   asm("_binary_zh_CN_connect_error_ogg_end");
        extern const char ogg_en_us_connect_error_start[] asm("_binary_en_US_connect_error_ogg_start");
        extern const char ogg_en_us_connect_error_end[]   asm("_binary_en_US_connect_error_ogg_end");
        inline std::string_view OGG_CONNECT_ERROR() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_connect_error_start), static_cast<size_t>(ogg_en_us_connect_error_end - ogg_en_us_connect_error_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_connect_error_start), static_cast<size_t>(ogg_zh_cn_connect_error_end - ogg_zh_cn_connect_error_start)};
            }
            return {};
        }

        // Sound: connecting
        extern const char ogg_zh_cn_connecting_start[] asm("_binary_zh_CN_connecting_ogg_start");
        extern const char ogg_zh_cn_connecting_end[]   asm("_binary_zh_CN_connecting_ogg_end");
        extern const char ogg_en_us_connecting_start[] asm("_binary_en_US_connecting_ogg_start");
        extern const char ogg_en_us_connecting_end[]   asm("_binary_en_US_connecting_ogg_end");
        inline std::string_view OGG_CONNECTING() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_connecting_start), static_cast<size_t>(ogg_en_us_connecting_end - ogg_en_us_connecting_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_connecting_start), static_cast<size_t>(ogg_zh_cn_connecting_end - ogg_zh_cn_connecting_start)};
            }
            return {};
        }

        // Sound: enter_remote
        extern const char ogg_zh_cn_enter_remote_start[] asm("_binary_zh_CN_enter_remote_ogg_start");
        extern const char ogg_zh_cn_enter_remote_end[]   asm("_binary_zh_CN_enter_remote_ogg_end");
        extern const char ogg_en_us_enter_remote_start[] asm("_binary_en_US_enter_remote_ogg_start");
        extern const char ogg_en_us_enter_remote_end[]   asm("_binary_en_US_enter_remote_ogg_end");
        inline std::string_view OGG_ENTER_REMOTE() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_enter_remote_start), static_cast<size_t>(ogg_en_us_enter_remote_end - ogg_en_us_enter_remote_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_enter_remote_start), static_cast<size_t>(ogg_zh_cn_enter_remote_end - ogg_zh_cn_enter_remote_start)};
            }
            return {};
        }

        // Sound: exit_remote
        extern const char ogg_zh_cn_exit_remote_start[] asm("_binary_zh_CN_exit_remote_ogg_start");
        extern const char ogg_zh_cn_exit_remote_end[]   asm("_binary_zh_CN_exit_remote_ogg_end");
        extern const char ogg_en_us_exit_remote_start[] asm("_binary_en_US_exit_remote_ogg_start");
        extern const char ogg_en_us_exit_remote_end[]   asm("_binary_en_US_exit_remote_ogg_end");
        inline std::string_view OGG_EXIT_REMOTE() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_exit_remote_start), static_cast<size_t>(ogg_en_us_exit_remote_end - ogg_en_us_exit_remote_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_exit_remote_start), static_cast<size_t>(ogg_zh_cn_exit_remote_end - ogg_zh_cn_exit_remote_start)};
            }
            return {};
        }

        // Sound: hi
        extern const char ogg_zh_cn_hi_start[] asm("_binary_zh_CN_hi_ogg_start");
        extern const char ogg_zh_cn_hi_end[]   asm("_binary_zh_CN_hi_ogg_end");
        extern const char ogg_en_us_hi_start[] asm("_binary_en_US_hi_ogg_start");
        extern const char ogg_en_us_hi_end[]   asm("_binary_en_US_hi_ogg_end");
        inline std::string_view OGG_HI() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_hi_start), static_cast<size_t>(ogg_en_us_hi_end - ogg_en_us_hi_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_hi_start), static_cast<size_t>(ogg_zh_cn_hi_end - ogg_zh_cn_hi_start)};
            }
            return {};
        }

        // Sound: open_aec
        extern const char ogg_zh_cn_open_aec_start[] asm("_binary_zh_CN_open_aec_ogg_start");
        extern const char ogg_zh_cn_open_aec_end[]   asm("_binary_zh_CN_open_aec_ogg_end");
        extern const char ogg_en_us_open_aec_start[] asm("_binary_en_US_open_aec_ogg_start");
        extern const char ogg_en_us_open_aec_end[]   asm("_binary_en_US_open_aec_ogg_end");
        inline std::string_view OGG_OPEN_AEC() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_open_aec_start), static_cast<size_t>(ogg_en_us_open_aec_end - ogg_en_us_open_aec_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_open_aec_start), static_cast<size_t>(ogg_zh_cn_open_aec_end - ogg_zh_cn_open_aec_start)};
            }
            return {};
        }

        // Sound: pain
        extern const char ogg_zh_cn_pain_start[] asm("_binary_zh_CN_pain_ogg_start");
        extern const char ogg_zh_cn_pain_end[]   asm("_binary_zh_CN_pain_ogg_end");
        extern const char ogg_en_us_pain_start[] asm("_binary_en_US_pain_ogg_start");
        extern const char ogg_en_us_pain_end[]   asm("_binary_en_US_pain_ogg_end");
        inline std::string_view OGG_PAIN() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_pain_start), static_cast<size_t>(ogg_en_us_pain_end - ogg_en_us_pain_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_pain_start), static_cast<size_t>(ogg_zh_cn_pain_end - ogg_zh_cn_pain_start)};
            }
            return {};
        }

        // Sound: upgrade
        extern const char ogg_zh_cn_upgrade_start[] asm("_binary_zh_CN_upgrade_ogg_start");
        extern const char ogg_zh_cn_upgrade_end[]   asm("_binary_zh_CN_upgrade_ogg_end");
        extern const char ogg_en_us_upgrade_start[] asm("_binary_en_US_upgrade_ogg_start");
        extern const char ogg_en_us_upgrade_end[]   asm("_binary_en_US_upgrade_ogg_end");
        inline std::string_view OGG_UPGRADE() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_upgrade_start), static_cast<size_t>(ogg_en_us_upgrade_end - ogg_en_us_upgrade_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_upgrade_start), static_cast<size_t>(ogg_zh_cn_upgrade_end - ogg_zh_cn_upgrade_start)};
            }
            return {};
        }

        // Sound: welcome
        extern const char ogg_zh_cn_welcome_start[] asm("_binary_zh_CN_welcome_ogg_start");
        extern const char ogg_zh_cn_welcome_end[]   asm("_binary_zh_CN_welcome_ogg_end");
        extern const char ogg_en_us_welcome_start[] asm("_binary_en_US_welcome_ogg_start");
        extern const char ogg_en_us_welcome_end[]   asm("_binary_en_US_welcome_ogg_end");
        inline std::string_view OGG_WELCOME() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_welcome_start), static_cast<size_t>(ogg_en_us_welcome_end - ogg_en_us_welcome_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_welcome_start), static_cast<size_t>(ogg_zh_cn_welcome_end - ogg_zh_cn_welcome_start)};
            }
            return {};
        }

        // Sound: wifi_success
        extern const char ogg_zh_cn_wifi_success_start[] asm("_binary_zh_CN_wifi_success_ogg_start");
        extern const char ogg_zh_cn_wifi_success_end[]   asm("_binary_zh_CN_wifi_success_ogg_end");
        extern const char ogg_en_us_wifi_success_start[] asm("_binary_en_US_wifi_success_ogg_start");
        extern const char ogg_en_us_wifi_success_end[]   asm("_binary_en_US_wifi_success_ogg_end");
        inline std::string_view OGG_WIFI_SUCCESS() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_wifi_success_start), static_cast<size_t>(ogg_en_us_wifi_success_end - ogg_en_us_wifi_success_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_wifi_success_start), static_cast<size_t>(ogg_zh_cn_wifi_success_end - ogg_zh_cn_wifi_success_start)};
            }
            return {};
        }

        // Sound: wificonfig
        extern const char ogg_zh_cn_wificonfig_start[] asm("_binary_zh_CN_wificonfig_ogg_start");
        extern const char ogg_zh_cn_wificonfig_end[]   asm("_binary_zh_CN_wificonfig_ogg_end");
        extern const char ogg_en_us_wificonfig_start[] asm("_binary_en_US_wificonfig_ogg_start");
        extern const char ogg_en_us_wificonfig_end[]   asm("_binary_en_US_wificonfig_ogg_end");
        inline std::string_view OGG_WIFICONFIG() {
            switch (GetLanguage()) {
                case Code::en_US:
                    return {static_cast<const char*>(ogg_en_us_wificonfig_start), static_cast<size_t>(ogg_en_us_wificonfig_end - ogg_en_us_wificonfig_start)};
                case Code::zh_CN:
                    return {static_cast<const char*>(ogg_zh_cn_wificonfig_start), static_cast<size_t>(ogg_zh_cn_wificonfig_end - ogg_zh_cn_wificonfig_start)};
            }
            return {};
        }

        // Common sound: exclamation
        extern const char ogg_exclamation_start[] asm("_binary_exclamation_ogg_start");
        extern const char ogg_exclamation_end[]   asm("_binary_exclamation_ogg_end");
        inline std::string_view OGG_EXCLAMATION() {
            return {static_cast<const char*>(ogg_exclamation_start), static_cast<size_t>(ogg_exclamation_end - ogg_exclamation_start)};
        }

        // Common sound: low_battery
        extern const char ogg_low_battery_start[] asm("_binary_low_battery_ogg_start");
        extern const char ogg_low_battery_end[]   asm("_binary_low_battery_ogg_end");
        inline std::string_view OGG_LOW_BATTERY() {
            return {static_cast<const char*>(ogg_low_battery_start), static_cast<size_t>(ogg_low_battery_end - ogg_low_battery_start)};
        }

        // Common sound: message_send
        extern const char ogg_message_send_start[] asm("_binary_message_send_ogg_start");
        extern const char ogg_message_send_end[]   asm("_binary_message_send_ogg_end");
        inline std::string_view OGG_MESSAGE_SEND() {
            return {static_cast<const char*>(ogg_message_send_start), static_cast<size_t>(ogg_message_send_end - ogg_message_send_start)};
        }

        // Common sound: over
        extern const char ogg_over_start[] asm("_binary_over_ogg_start");
        extern const char ogg_over_end[]   asm("_binary_over_ogg_end");
        inline std::string_view OGG_OVER() {
            return {static_cast<const char*>(ogg_over_start), static_cast<size_t>(ogg_over_end - ogg_over_start)};
        }

        // Common sound: popup
        extern const char ogg_popup_start[] asm("_binary_popup_ogg_start");
        extern const char ogg_popup_end[]   asm("_binary_popup_ogg_end");
        inline std::string_view OGG_POPUP() {
            return {static_cast<const char*>(ogg_popup_start), static_cast<size_t>(ogg_popup_end - ogg_popup_start)};
        }

        // Common sound: success
        extern const char ogg_success_start[] asm("_binary_success_ogg_start");
        extern const char ogg_success_end[]   asm("_binary_success_ogg_end");
        inline std::string_view OGG_SUCCESS() {
            return {static_cast<const char*>(ogg_success_start), static_cast<size_t>(ogg_success_end - ogg_success_start)};
        }

        // Common sound: vibration
        extern const char ogg_vibration_start[] asm("_binary_vibration_ogg_start");
        extern const char ogg_vibration_end[]   asm("_binary_vibration_ogg_end");
        inline std::string_view OGG_VIBRATION() {
            return {static_cast<const char*>(ogg_vibration_start), static_cast<size_t>(ogg_vibration_end - ogg_vibration_start)};
        }

        // Board-specific sound: arm_startup
        extern const char ogg_arm_startup_start[] asm("_binary_arm_startup_ogg_start");
        extern const char ogg_arm_startup_end[]   asm("_binary_arm_startup_ogg_end");
        inline std::string_view OGG_ARM_STARTUP() {
            return {static_cast<const char*>(ogg_arm_startup_start), static_cast<size_t>(ogg_arm_startup_end - ogg_arm_startup_start)};
        }

        // Board-specific sound: engine_startup
        extern const char ogg_engine_startup_start[] asm("_binary_engine_startup_ogg_start");
        extern const char ogg_engine_startup_end[]   asm("_binary_engine_startup_ogg_end");
        inline std::string_view OGG_ENGINE_STARTUP() {
            return {static_cast<const char*>(ogg_engine_startup_start), static_cast<size_t>(ogg_engine_startup_end - ogg_engine_startup_start)};
        }

        // Board-specific sound: engine_throttle
        extern const char ogg_engine_throttle_start[] asm("_binary_engine_throttle_ogg_start");
        extern const char ogg_engine_throttle_end[]   asm("_binary_engine_throttle_ogg_end");
        inline std::string_view OGG_ENGINE_THROTTLE() {
            return {static_cast<const char*>(ogg_engine_throttle_start), static_cast<size_t>(ogg_engine_throttle_end - ogg_engine_throttle_start)};
        }

        // Board-specific sound: woof
        extern const char ogg_woof_start[] asm("_binary_woof_ogg_start");
        extern const char ogg_woof_end[]   asm("_binary_woof_ogg_end");
        inline std::string_view OGG_WOOF() {
            return {static_cast<const char*>(ogg_woof_start), static_cast<size_t>(ogg_woof_end - ogg_woof_start)};
        }
    }

}
