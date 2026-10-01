#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "audio_service.h"
#include "ble_remote_control.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "settings.h"
#include "esp32_camera.h"
#include "assets/lang_config.h"
#include "board_config.h"

#include "tars_display.h"

#include <wifi_manager.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <nvs_flash.h>
#include <esp_flash.h>
#include <esp_sleep.h>

#include <algorithm>

#include "robot.h"
#include "robot_action.h"
#include "imu.h"

#define TAG "TARS"

// 长按重置 NVS 的时间阈值（毫秒）
static constexpr int kLongPressResetMs = 3000;
static constexpr int kLongPressShowEmotionMs = 1000;  // 长按1秒后显示表情

// GPIO3 is an external digital touch module.  Long press intentionally enters
// deep sleep; a later short module pulse wakes the ESP32-S3.
static constexpr int kTouchLongPressSleepMs = 3000;
static constexpr int kTouchLongPressShowSleepMs = 1000;

class TarsBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    TarsDisplay* display_ = nullptr;
    esp_lcd_panel_handle_t lcd_panel_ = nullptr;
    Esp32Camera* camera_ = nullptr;  // 初始化为nullptr
    TaskHandle_t robot_task_handle_ = nullptr;
    TaskHandle_t motor_rx_task_handle_ = nullptr;
    int64_t button_press_start_time_ = 0;  // boot按键按下时间戳
    esp_timer_handle_t long_press_timer_ = nullptr;  // boot按键长按检测定时器
    bool nvs_reset_emotion_shown_ = false;  // boot按键是否已显示 nvs_reset 表情
    int64_t touch_press_start_time_ = 0;  // 触摸按下时间戳
    bool touch_long_press_seen_ = false;
    esp_timer_handle_t touch_long_press_timer_ = nullptr;
    int honesty_ = 90;
    int humor_ = 75;

    void LoadPersonality() {
        // NVS namespaces are limited to 15 characters.  The former
        // "tars_personality" name is 16 characters, so nvs_open() failed;
        // the following write then aborted through ESP_ERROR_CHECK and
        // restarted the device.  Keep this short, stable namespace for both
        // the read and write paths.
        Settings settings("tars_personal");
        honesty_ = std::clamp(static_cast<int>(settings.GetInt("honesty", 90)), 0, 100);
        humor_ = std::clamp(static_cast<int>(settings.GetInt("humor", 75)), 0, 100);
    }

    void SetPersonality(int honesty, int humor) {
        honesty_ = std::clamp(honesty, 0, 100);
        humor_ = std::clamp(humor, 0, 100);
        Settings settings("tars_personal", true);
        settings.SetInt("honesty", honesty_);
        settings.SetInt("humor", humor_);
        if (display_ != nullptr) {
            display_->SetPersonality(honesty_, humor_);
        }
    }

    void EnterTouchDeepSleep() {
        ESP_LOGI(TAG, "Touch long press: entering deep sleep, wake on GPIO%d LOW", TOUCH_BUTTON_GPIO);
        // 先挂起步态/回读任务，独占舵机 UART——否则 EnableAllMotor 的卸载
        // 帧会被周期性位置指令打散，舵机收不到有效命令而保持力矩。
        if (robot_task_handle_ != nullptr) {
            vTaskSuspend(robot_task_handle_);
        }
        if (motor_rx_task_handle_ != nullptr) {
            vTaskSuspend(motor_rx_task_handle_);
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // 等总线上最后一笔发送收尾
        // 挂起时若任务正停在发送中段，serial_lock 会卡在 1——清零否则
        // 接下来的卸载命令全部被 SendMotorCommand 丢弃
        serial_lock = 0;
        // 关机前卸载全部舵机（mode 0），避免 deep sleep 时电机仍带力耗电
        EnableAllMotor(0);
        if (lcd_panel_ != nullptr) {
            esp_err_t err = esp_lcd_panel_disp_on_off(lcd_panel_, false);
            if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(TAG, "Unable to turn LCD off: %s", esp_err_to_name(err));
            }
            err = esp_lcd_panel_disp_sleep(lcd_panel_, true);
            if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(TAG, "Unable to put LCD to sleep: %s", esp_err_to_name(err));
            }
        }

        // GPIO3 is an RTC-capable input.  Keep it pulled HIGH while asleep;
        // a short touch pulls it LOW and wakes the device through EXT0.
        ESP_ERROR_CHECK(rtc_gpio_init(TOUCH_BUTTON_GPIO));
        ESP_ERROR_CHECK(rtc_gpio_set_direction(TOUCH_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY));
        ESP_ERROR_CHECK(rtc_gpio_pullup_en(TOUCH_BUTTON_GPIO));
        ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(TOUCH_BUTTON_GPIO));
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(TOUCH_BUTTON_GPIO, 0));
        esp_deep_sleep_start();
    }

    void InitializeUart() {
        uart_config_t uart_cfg = {
            .baud_rate = 500000,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };
        uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
        uart_param_config(UART_NUM_2, &uart_cfg);
        uart_set_pin(UART_NUM_2, MOTOR_UART_TX_PIN, MOTOR_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    void InitializeLaser() {
        // esp_rom_gpio_pad_select_gpio(LASER_GPIO);
        // gpio_reset_pin(LASER_GPIO);
        // gpio_config_t io_conf = {
        //     .pin_bit_mask = (1ULL << LASER_GPIO),
        //     .mode = GPIO_MODE_OUTPUT,
        //     .pull_up_en = GPIO_PULLUP_DISABLE,
        //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
        //     .intr_type = GPIO_INTR_DISABLE,
        // };
        // gpio_config(&io_conf);
    }

    void InitializeBootButton() {
        esp_rom_gpio_pad_select_gpio(GPIO_NUM_0);
        gpio_reset_pin(GPIO_NUM_0);
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << GPIO_NUM_0),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        // TarsDisplay flushes 16 scan lines per transaction.  A full-frame
        // max_transfer_sz makes the SPI driver allocate a 153.6 KB internal
        // DMA bounce buffer when the frame buffer lives in PSRAM, starving
        // BluFi's Bluetooth controller during first-time provisioning.
        buscfg.max_transfer_sz = TARS_DISPLAY_WIDTH * TARS_DISPLAY_DRAW_BUFFER_LINES * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = TARS_DISPLAY_PCLK_HZ;
        // Double-buffered renderer needs at most two outstanding transfers.
        // A deep queue only consumes scarce internal DMA descriptors.
        io_config.trans_queue_depth = TARS_DISPLAY_SPI_TRANS_QUEUE_DEPTH;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGI(TAG, "Install ST7789V2 320x240 landscape LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = TARS_DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, TARS_DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, TARS_DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, TARS_DISPLAY_MIRROR_X, TARS_DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        lcd_panel_ = panel;

        display_ = new TarsDisplay(panel, panel_io, TARS_DISPLAY_WIDTH, TARS_DISPLAY_HEIGHT);
        LoadPersonality();
        display_->SetPersonality(honesty_, humor_);
        ESP_LOGI(TAG, "Using TARS dual-panel status display");
    }

    void InitializeCamera() {
        camera_config_t camera_config = {
            .pin_pwdn = CAMERA_PIN_PWDN,
            .pin_reset = CAMERA_PIN_RESET,
            .pin_xclk = CAMERA_PIN_XCLK,
            .pin_sccb_sda = CAMERA_PIN_SIOD,
            .pin_sccb_scl = CAMERA_PIN_SIOC,
            .pin_d7 = CAMERA_PIN_D7,
            .pin_d6 = CAMERA_PIN_D6,
            .pin_d5 = CAMERA_PIN_D5,
            .pin_d4 = CAMERA_PIN_D4,
            .pin_d3 = CAMERA_PIN_D3,
            .pin_d2 = CAMERA_PIN_D2,
            .pin_d1 = CAMERA_PIN_D1,
            .pin_d0 = CAMERA_PIN_D0,
            .pin_vsync = CAMERA_PIN_VSYNC,
            .pin_href = CAMERA_PIN_HREF,
            .pin_pclk = CAMERA_PIN_PCLK,
            .xclk_freq_hz = XCLK_FREQ_HZ,
            .ledc_timer = LEDC_TIMER_0,
            .ledc_channel = LEDC_CHANNEL_0,
            .pixel_format = PIXFORMAT_RGB565,
            .frame_size = FRAMESIZE_240X240,
            .jpeg_quality = 12,
            .fb_count = 2,
            .fb_location = CAMERA_FB_IN_PSRAM,
            .grab_mode = CAMERA_GRAB_LATEST,  // 始终获取最新帧
            .sccb_i2c_port = 1,  // 摄像头用GPIO 4/5，IMU用GPIO 48/14，必须用不同端口
        };

        // 先检查摄像头是否可用
        sensor_t* sensor = esp_camera_sensor_get();
        if (sensor != nullptr) {
            // 传感器已存在，不需要重复初始化
            ESP_LOGW(TAG, "Camera sensor already initialized");
            camera_ = nullptr;
            return;
        }

        camera_ = new Esp32Camera(camera_config);
        
        // 检查摄像头传感器是否成功初始化
        sensor = esp_camera_sensor_get();
        if (sensor != nullptr) {
            ESP_LOGI(TAG, "Camera initialized successfully, sensor PID: 0x%x", sensor->id.PID);
            // 摄像头 180° 旋转
            sensor->set_hmirror(sensor, 1);
            sensor->set_vflip(sensor, 1);
        } else {
            ESP_LOGW(TAG, "Camera initialization failed, camera tools will not be available");
            // 注意: 这里不删除 camera_ 以避免多态类型析构警告
            // 内存泄漏量很小（对象本身很小），且只会发生一次
            camera_ = nullptr;
        }
    }

    void InitializeButtons() {
        // 创建长按检测定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto board = static_cast<TarsBoard*>(arg);
                // 检查按键是否仍被按住
                if (board->button_press_start_time_ > 0) {
                    ESP_LOGI(TAG, "Long press detected (>1s), showing nvs_reset emotion");
                    if (board->display_) {
                        board->display_->SetEmotion("nvs_reset");
                    }
                    board->nvs_reset_emotion_shown_ = true;
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "long_press_timer",
        };
        esp_timer_create(&timer_args, &long_press_timer_);

        // 记录按键按下时间，并启动定时器
        boot_button_.OnPressDown([this]() {
            // 标定模式下不处理长按
            if (calibrate_mode == 1) {
                return;
            }
            button_press_start_time_ = esp_timer_get_time() / 1000;  // 转换为毫秒
            nvs_reset_emotion_shown_ = false;
            ESP_LOGI(TAG, "Button pressed down");
            // 启动1秒定时器，检测长按
            esp_timer_start_once(long_press_timer_, kLongPressShowEmotionMs * 1000);
        });

        // 检查长按时间
        boot_button_.OnPressUp([this]() {
            // 停止定时器
            esp_timer_stop(long_press_timer_);
            
            if (button_press_start_time_ > 0) {
                int64_t press_duration = (esp_timer_get_time() / 1000) - button_press_start_time_;
                ESP_LOGI(TAG, "Button released, press duration: %lld ms", (long long)press_duration);
                
                if (press_duration >= kLongPressResetMs) {
                    ESP_LOGW(TAG, "Long press detected (>3s), resetting NVS...");
                    // 播放提示音（如果可用）
                    auto& app = Application::GetInstance();
                    app.PlaySound(Lang::Sounds::OGG_TARS_SUCCESS());
                    vTaskDelay(pdMS_TO_TICKS(500));
                    
                    // 清除 NVS
                    esp_err_t ret = nvs_flash_erase();
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "NVS erased successfully");
                    } else {
                        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
                    }
                    nvs_flash_init();
                    
                    // 重启设备
                    ESP_LOGI(TAG, "Restarting device in 1 second...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else if (nvs_reset_emotion_shown_) {
                    // 未达到重置时间，但已显示了表情，恢复正常表情
                    ESP_LOGI(TAG, "Long press cancelled, restoring emotion");
                    if (display_) {
                        display_->SetEmotion("neutral");
                    }
                }
                button_press_start_time_ = 0;
                nvs_reset_emotion_shown_ = false;
            }
        });

        // 保持原有的单击功能
        boot_button_.OnClick([this]() {
            // 标定模式下不处理单击
            if (calibrate_mode == 1) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });

        // Touch module is a conventional active-low GPIO signal, not an ESP
        // touch-pad channel.  Its long-press timer is separate from the boot
        // button's NVS-reset timer.
        esp_timer_create_args_t touch_timer_args = {
            .callback = [](void* arg) {
                auto* board = static_cast<TarsBoard*>(arg);
                if (board->touch_press_start_time_ > 0) {
                    board->touch_long_press_seen_ = true;
                    if (board->display_) {
                        board->display_->SetEmotion("sleepy");
                    }
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "touch_sleep",
        };
        ESP_ERROR_CHECK(esp_timer_create(&touch_timer_args, &touch_long_press_timer_));

        touch_button_.OnPressDown([this]() {
            touch_press_start_time_ = esp_timer_get_time() / 1000;
            touch_long_press_seen_ = false;
            ESP_ERROR_CHECK(esp_timer_start_once(touch_long_press_timer_,
                                                 kTouchLongPressShowSleepMs * 1000));
        });

        touch_button_.OnPressUp([this]() {
            // Stopping an expired one-shot timer can return INVALID_STATE;
            // that only means the long-press prompt was already shown.
            esp_timer_stop(touch_long_press_timer_);
            if (touch_press_start_time_ > 0) {
                int64_t press_duration = (esp_timer_get_time() / 1000) - touch_press_start_time_;
                ESP_LOGI(TAG, "Touch released, duration: %lld ms", (long long)press_duration);
                if (press_duration >= kTouchLongPressSleepMs) {
                    touch_long_press_seen_ = true;
                    touch_press_start_time_ = 0;
                    EnterTouchDeepSleep();
                } else if (touch_long_press_seen_ && display_) {
                    display_->SetEmotion("neutral");
                }
                touch_press_start_time_ = 0;
                touch_long_press_seen_ = false;
            }
        });

        // During first-boot motion calibration, the external GPIO3 touch
        // sensor provides the explicit confirmation action.  A short touch
        // stores the current zero positions and lets CheckCalibration()
        // continue startup.  Outside calibration a short touch deliberately
        // remains a no-op; only a 3-second hold enters deep sleep.
        touch_button_.OnClick([this]() {
            if (calibrate_mode != 1) {
                return;
            }
            ESP_LOGI(TAG, "Touch click: confirming and exiting calibration mode");
            Calibrate(0);
        });
    }

    void Calibrate(int mode) {
        short mid_pos[] = {M_N/2, M_N/2, M_N/2};
        if(mode==1 && calibrate_mode==0){
            calibrate_mode = 1;
            if (display_) {
                display_->HideCalibrationError();
            }
            for(int i=0;i<5;i++){
                SetMotorPos(mid_pos, 500);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            EnableAllMotor(0);
            if (display_) {
                display_->SetEmotion("calibration");
            }
        }
        if(mode==0 && calibrate_mode==1){
            if (!WriteZeroPos()) {
                // 写入失败：屏幕显示红色圆环 + 感叹号图标，底部一行显示原因，并留在标定模式
                if (display_) {
                    display_->ShowCalibrationError(g_calib_error_msg);
                }
                return;
            }
            if (display_) {
                display_->HideCalibrationError();
            }
            EnableAllMotor(1);
            calibrate_mode = 0;
            if (display_) {
                display_->SetEmotion("neutral");
            }
        }
    }

    enum class GpioMode {
        Off = 0,
        On = 1,
        Toggle = 2
    };

    void ControlLaser(GpioMode mode) {
        // switch (mode) {
        //     case GpioMode::Off:
        //         gpio_set_level(LASER_GPIO, 0);
        //         break;
        //     case GpioMode::On:
        //         gpio_set_level(LASER_GPIO, 1);
        //         break;
        //     case GpioMode::Toggle:
        //         gpio_set_level(LASER_GPIO, 0);
        //         ESP_LOGI(TAG, "Switch lighting modes");
        //         gpio_set_level(LASER_GPIO, 1);
        //         break;
        // }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.tars.set_personality",
            "设置 TARS 人格参数。用户要求调整诚实度或幽默度时调用；范围为 0-100，修改后会"
            "立即显示并永久保存。未需要修改的一项传 -1。",
            PropertyList({
                Property("honesty", kPropertyTypeInteger, -1, -1, 100),
                Property("humor", kPropertyTypeInteger, -1, -1, 100),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                const int requested_honesty = properties["honesty"].value<int>();
                const int requested_humor = properties["humor"].value<int>();
                if (requested_honesty < 0 && requested_humor < 0) {
                    return std::string("请给出诚实度或幽默度，范围为 0 到 100。");
                }
                SetPersonality(requested_honesty < 0 ? honesty_ : requested_honesty,
                               requested_humor < 0 ? humor_ : requested_humor);
                return std::string("TARS 人格已更新：诚实度 ") + std::to_string(honesty_) +
                       "%；幽默度 " + std::to_string(humor_) + "%";
            });

        mcp_server.AddTool("self.tars.forward",
            "TARS 前进或后退。speed 为运动幅度(0-60，正值前进，负值后退，0 停下)，"
            "time 为持续时间(毫秒)，time=0 时持续运动直到再次调用并设 speed=0。"
            "步态频率 1.5Hz。适用于用户说'前进''往前走''后退''过来'等。",
            PropertyList({
                Property("speed", kPropertyTypeInteger, -60, 60),
                Property("time", kPropertyTypeInteger, 0, 10000),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int speed = properties["speed"].value<int>();
                int time = properties["time"].value<int>();
                ESP_LOGI(TAG, "MCP forward speed=%d time=%d", speed, time);
                tars_set_forward(speed, time);
                return true;
            });

        mcp_server.AddTool("self.tars.turn",
            "TARS 左转或右转。speed 为运动幅度(0-60，正值左转，负值右转，0 停下)，"
            "time 为持续时间(毫秒)，time=0 时持续运动直到再次调用并设 speed=0。"
            "步态频率 1.5Hz。适用于用户说'左转''右转''转个圈''掉头'等。",
            PropertyList({
                Property("speed", kPropertyTypeInteger, -60, 60),
                Property("time", kPropertyTypeInteger, 0, 10000),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int speed = properties["speed"].value<int>();
                int time = properties["time"].value<int>();
                ESP_LOGI(TAG, "MCP turn speed=%d time=%d", speed, time);
                tars_set_turn(speed, time);
                return true;
            });

        mcp_server.AddTool("self.tars.calibrate",
            "标定 TARS,1为进入标定,0为退出/完成标定",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 1),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int mode = properties["mode"].value<int>();
                ESP_LOGI(TAG, "Calibrate called with mode=%d", mode);
                Calibrate(mode);
                return true;
            });

        mcp_server.AddTool("self.laser.control",
            "激光剑控制: 0=关闭, 1=打开, 2=切换激光剑模式",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 2),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int modeValue = properties["mode"].value<int>();
                if (modeValue < 0 || modeValue > 2) {
                    ESP_LOGE(TAG, "Invalid mode value: %d", modeValue);
                    return false;
                }
                ControlLaser(static_cast<GpioMode>(modeValue));
                return true;
            });

        mcp_server.AddTool("self.ble.remote_control",
            "开启/关闭蓝牙遥控模式。开启后可用小程序或 APP 遥控（蓝牙名称与配网时相同）。"
            "enable=1 开启遥控模式, enable=0 关闭遥控模式",
            PropertyList({
                Property("enable", kPropertyTypeInteger, 0, 1),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int enable = properties["enable"].value<int>();
                if (enable) {
                    if (!ble_remote_is_running()) {
                        ESP_LOGI(TAG, "Starting BLE remote control mode");
                        if (display_) {
                            display_->SetEmotion("remote_mode");
                        }
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_TARS_ENTER_REMOTE());

                        bool success = ble_remote_init();
                        if (success) {
                            ESP_LOGI(TAG, "BLE remote control started");
                            return std::string("蓝牙遥控模式已开启，请用小程序或 APP 连接");
                        } else {
                            ESP_LOGE(TAG, "Failed to start BLE remote control");
                            return std::string("蓝牙遥控模式启动失败");
                        }
                    }
                    return std::string("蓝牙遥控模式已经开启");
                } else {
                    if (ble_remote_is_running()) {
                        ble_remote_deinit();
                        ESP_LOGI(TAG, "BLE remote control stopped");
                        if (display_) {
                            display_->SetEmotion("neutral");
                        }
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_TARS_EXIT_REMOTE());
                    }
                    return std::string("蓝牙遥控模式已关闭");
                }
            });
    }

public:
    TarsBoard() : boot_button_(BOOT_BUTTON_GPIO),
                  touch_button_(TOUCH_BUTTON_GPIO, TOUCH_BUTTON_ACTIVE_HIGH) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        
        InitializeCamera();
        
        InitializeUart();
        
        InitZeroPos();

        // 立即读取一次电池电压，避免等待 60 秒才有电量数据
        ReadServoVoltage(1);
        
        EnableStallDetection(true);
        
        InitializeBootButton();
        
        // IMU 初始化；若传感器在上电初期尚未准备好，接收任务会继续重试。
        imu_init();
        // 机器人控制任务
        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                robot_control();
                vTaskDelay(pdMS_TO_TICKS(ROBOT_TASK_INTERVAL_MS));
            }
            vTaskDelete(NULL);
        }, "robot_task", 4096, this, 5, &robot_task_handle_, 0);

        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            uint32_t imu_retry_count = 0;
            while (true) {
                motor_rx();
                if (!imu_is_initialized() && (++imu_retry_count % 50) == 0) {
                    ESP_LOGW(TAG, "IMU unavailable, retrying I2C initialization");
                    imu_init();
                }
                imu_read_once();
                vTaskDelay(pdMS_TO_TICKS(MOTOR_RX_TASK_INTERVAL_MS));
            }
            vTaskDelete(NULL);
        }, "motor_rx_task", 4096, this, 5, &motor_rx_task_handle_, 1);
        ESP_LOGI(TAG, "Robot control tasks created");
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual std::string GetDeviceStatusJson() override {
        std::string base_json = WifiBoard::GetDeviceStatusJson();
        cJSON* root = cJSON_Parse(base_json.c_str());
        if (!root) {
            return base_json;
        }

        imu_read_once();

        cJSON* imu = cJSON_CreateObject();
        cJSON_AddBoolToObject(imu, "initialized", imu_is_initialized());
        cJSON_AddNumberToObject(imu, "roll", roll);
        cJSON_AddNumberToObject(imu, "pitch", pitch);
        cJSON_AddNumberToObject(imu, "yaw", yaw);
        cJSON_AddItemToObject(root, "imu", imu);

        cJSON* personality = cJSON_CreateObject();
        cJSON_AddNumberToObject(personality, "honesty", honesty_);
        cJSON_AddNumberToObject(personality, "humor", humor_);
        cJSON_AddItemToObject(root, "personality", personality);

        char* json_str = cJSON_PrintUnformatted(root);
        std::string result(json_str);
        cJSON_free(json_str);
        cJSON_Delete(root);

        return result;
    }

    virtual void OnStartup() override {
        display_->Begin();
        display_->SetEmotion("launch");
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_TARS_STARTUP());
        ESP_LOGI(TAG, "Boot animation: launch + tars_startup");
    }

    // 语音/MCP 关机入口：复用触摸长按的完整关机流程（卸载舵机+关屏+deep sleep）
    virtual void EnterDeepSleep() override {
        EnterTouchDeepSleep();
    }

    virtual void OnInitializationComplete() override {
        ESP_LOGI(TAG, "Initialization complete");
    }

    void SetLaser(bool on) override {
        (void)on;
    }

    bool GetLaser() override {
        return false;
    }

    virtual void OnWifiConfigStart() override {
        ESP_LOGI(TAG, "WiFi config start, reset to stand then keep sit");
    }

    virtual void OnWifiConfigEnd() override {
        // 配网结束，从坐姿起身并播放成功语音
        if (display_ != nullptr) {
            display_->SetEmotion("neutral");
        }
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_TARS_WIFI_SUCCESS());
        ESP_LOGI(TAG, "WiFi config end, sit reset (stand up) and play success audio");
    }

    virtual void CheckCalibration(Display* display, AudioService& audio) override {
        // 检查是否需要标定
        if (calibrate_mode != 1) {
            ESP_LOGI(TAG, "Device already calibrated, skipping calibration");
            return;
        }
        
        ESP_LOGW(TAG, "Device needs calibration, entering calibration mode");
        
        // 重新进入标定：清掉上一次的失败提示图标
        if (display_) {
            display_->HideCalibrationError();
        }

        // 显示标定表情
        if (display) {
            display->SetEmotion("calibration");
        }
        
        // 播放进入标定语音
        audio.PlaySound(Lang::Sounds::OGG_TARS_CALIBRATION_ENTER());

        // 阻塞等待标定完成（Touch 单击确认并退出标定模式）
        ESP_LOGI(TAG, "Waiting for calibration... (touch to exit)");
        while (calibrate_mode == 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        ESP_LOGI(TAG, "Calibration completed!");
        
        // 标定完成，启用舵机
        for (int i = 0; i < MOTOR_NUM; i++) {
            motor[i].Load = 1;
        }
        
        // 播放退出标定语音
        audio.PlaySound(Lang::Sounds::OGG_TARS_CALIBRATION_EXIT());
        
        // 恢复正常表情
        if (display) {
            display->SetEmotion("neutral");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等待语音播放
    }

    // 从舵机 ID=1 的 PRESENT_VOLTAGE 寄存器读取电池电压
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        float voltage = servo_voltage;
        if (voltage < 0.1f) return false;  // 尚未读取到有效电压
        
        // 2S 锂电池: 6.6V(~0%) ~ 8.4V(100%)
        if (voltage >= 8.4f)
            level = 100;
        else if (voltage <= 6.6f)
            level = 0;
        else
            level = (int)((voltage - 6.6f) / (8.4f - 6.6f) * 100.0f);
        
        charging = false;
        discharging = true;
        return true;
    }

    virtual std::string GetBoardDescription() override {
        return "一个三舵机 TARS 形态机器人，搭载横屏 320x240 ST7789V2 LCD、ESP32-S3 MCU、8MB PSRAM、"
               "3路智能舵机、IMU 姿态传感器、GC0308 摄像头。"
               "2S 锂电池供电(6.6V-8.4V)，通过舵机 ID=1 读取电压监测电量。";
    }

    virtual std::string_view GetSuccessSound() override {
        return Lang::Sounds::OGG_TARS_SUCCESS();
    }
};

DECLARE_BOARD(TarsBoard);
