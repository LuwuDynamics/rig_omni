#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "esp32_camera.h"
// #include "camera_web_server.h"  // TODO: Migrate camera web server if needed
// #include "lulu_ble.h"  // 暂时屏蔽，启用 BluFi 配网
#include "assets/lang_config.h"
#include "board_config.h"

#include "display/emote_display.h"

#include <wifi_manager.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <nvs_flash.h>

#include "esp_lcd_gc9a01.h"
#include "xgo.h"
#include "xgo_action.h"
#include "imu.h"
#include "hover_debug_server.h"

#define TAG "HOVER"

// Hover 专属音效
namespace {
    // 开机音效 (0~3s)
    extern const char engine_startup_ogg_start[] asm("_binary_engine_startup_ogg_start");
    extern const char engine_startup_ogg_end[] asm("_binary_engine_startup_ogg_end");
    static const std::string_view STARTUP_SOUND {
        static_cast<const char*>(engine_startup_ogg_start),
        static_cast<size_t>(engine_startup_ogg_end - engine_startup_ogg_start)
    };
    // 油门音效 (8s~结尾)
    extern const char engine_throttle_ogg_start[] asm("_binary_engine_throttle_ogg_start");
    extern const char engine_throttle_ogg_end[] asm("_binary_engine_throttle_ogg_end");
    static const std::string_view THROTTLE_SOUND {
        static_cast<const char*>(engine_throttle_ogg_start),
        static_cast<size_t>(engine_throttle_ogg_end - engine_throttle_ogg_start)
    };
}

// 长按重置 NVS 的时间阈值（毫秒）
static constexpr int kLongPressResetMs = 3000;
static constexpr int kLongPressShowEmotionMs = 1000;  // 长按1秒后显示表情
static constexpr int kDoubleTapWindowMs = 400;  // 双击检测窗口（毫秒）

class HoverBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    emote::EmoteDisplay* display_ = nullptr;  // AAF动画显示
    Esp32Camera* camera_ = nullptr;  // 初始化为nullptr
    TaskHandle_t xgo_task_handle_ = nullptr;
    TaskHandle_t xgo_rx_task_handle_ = nullptr;
    TaskHandle_t imu_task_handle_ = nullptr;
    int64_t button_press_start_time_ = 0;  // 按键按下时间戳
    esp_timer_handle_t long_press_timer_ = nullptr;  // 长按检测定时器
    bool nvs_reset_emotion_shown_ = false;  // 是否已显示 nvs_reset 表情

    // 触摸双击检测
    int touch_tap_count_ = 0;                    // 触摸点击计数
    esp_timer_handle_t tap_timer_ = nullptr;    // 双击检测定时器

    void InitializeUart() {
        uart_config_t uart_cfg = {
            .baud_rate = 1000000,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };
        ESP_LOGI(TAG, "Initialize UART");
        uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
        uart_param_config(UART_NUM_2, &uart_cfg);
        uart_set_pin(UART_NUM_2, XGO_UART_TX_PIN, XGO_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        WriteByte_P_V(3, 1500, 1300);
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
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
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
        io_config.pclk_hz = 80 * 1000 * 1000;  // 80MHz SPI
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGI(TAG, "Install GC9A01 LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        // 使用默认GC9A01初始化，不使用自定义gc9107命令
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示

        // 使用 EmoteDisplay 播放 AAF 动画
        display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        ESP_LOGI(TAG, "Using EmoteDisplay for AAF animations");
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
        } else {
            ESP_LOGW(TAG, "Camera initialization failed, camera tools will not be available");
            // 注意: 这里不删除 camera_ 以避免多态类型析构警告
            // 内存泄漏量很小（对象本身很小），且只会发生一次
            camera_ = nullptr;
        }
    }



    void InitializeButtons() {
        // 创建双击检测定时器
        esp_timer_create_args_t tap_timer_args = {
            .callback = [](void* arg) {
                auto board = static_cast<HoverBoard*>(arg);
                if (board->touch_tap_count_ == 1) {
                    // 单触：随机摇头 + 引擎启动声
                    int angle = (esp_random() % 150) - 75;  // -75 ~ 75 度
                    target_head_pos = angle;
                    auto& app = Application::GetInstance();
                    app.PlaySound(STARTUP_SOUND);
                    ESP_LOGI(TAG, "Single tap: random head shake to %d deg + startup sound", angle);
                } else if (board->touch_tap_count_ >= 2) {
                    // 双击：播放油门音效 + 原地转圈
                    auto& app = Application::GetInstance();
                    app.PlaySound(THROTTLE_SOUND);
                    stable_yaw += 360.0f;
                    yaw_ctrl_time = esp_timer_get_time() / 1000.0;
                    ESP_LOGI(TAG, "Double tap: spin 360° + throttle sound");
                }
                board->touch_tap_count_ = 0;
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "tap_timer",
        };
        esp_timer_create(&tap_timer_args, &tap_timer_);

        // 创建长按检测定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto board = static_cast<HoverBoard*>(arg);
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
                ESP_LOGI(TAG, "Button released, press duration: %d ms", press_duration);
                
                if (press_duration >= kLongPressResetMs) {
                    ESP_LOGW(TAG, "Long press detected (>3s), resetting NVS...");
                    // 播放提示音（如果可用）
                    auto& app = Application::GetInstance();
                    app.PlaySound(THROTTLE_SOUND);
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
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });

        // Touch 传感器 —— 单触随机摇头，双击转圈 + 音效
        touch_button_.OnPressDown([this]() {
            button_press_start_time_ = esp_timer_get_time() / 1000;
            nvs_reset_emotion_shown_ = false;
            ESP_LOGI(TAG, "Touch pressed down");
            esp_timer_start_once(long_press_timer_, kLongPressShowEmotionMs * 1000);
        });

        touch_button_.OnPressUp([this]() {
            esp_timer_stop(long_press_timer_);
            if (button_press_start_time_ > 0) {
                int64_t press_duration = (esp_timer_get_time() / 1000) - button_press_start_time_;
                ESP_LOGI(TAG, "Touch released, press duration: %d ms", press_duration);
                if (press_duration >= kLongPressResetMs) {
                    // 长按 >3s：重置 NVS
                    ESP_LOGW(TAG, "Touch long press detected (>3s), resetting NVS...");
                    auto& app = Application::GetInstance();
                    app.PlaySound(THROTTLE_SOUND);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_err_t ret = nvs_flash_erase();
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "NVS erased successfully");
                    } else {
                        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
                    }
                    nvs_flash_init();
                    ESP_LOGI(TAG, "Restarting device in 1 second...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else if (nvs_reset_emotion_shown_) {
                    // 长按 >1s 但 <3s：取消，恢复表情
                    ESP_LOGI(TAG, "Touch long press cancelled, restoring emotion");
                    if (display_) {
                        display_->SetEmotion("neutral");
                    }
                } else {
                    // 短按：检测单/双击
                    touch_tap_count_++;
                    if (touch_tap_count_ == 1) {
                        // 启动双击检测定时器
                        esp_timer_start_once(tap_timer_, kDoubleTapWindowMs * 1000);
                    }
                }
                button_press_start_time_ = 0;
                nvs_reset_emotion_shown_ = false;
            }
        });
    }

    void SetDogSpeed(int dog_vx, int dog_vyaw, int time) {
        ESP_LOGI(TAG, "SetDogSpeed: vx=%d, vyaw=%d, time=%d", dog_vx, dog_vyaw, time);

        vx = int(2.2 * dog_vx);
        vyaw = int(2.8 * dog_vyaw);
        if (time > 0) {
            vTaskDelay(pdMS_TO_TICKS(time));
        }
        vx = 0.0;
        vyaw = 0.0;
        ESP_LOGI(TAG, "SetDogSpeed: done");
    }


    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.robot.head_angle",
            "设置机器头部角度,角度正负45,0为中间,正为左扭头,负为右扭头",
            PropertyList({
                Property("angle", kPropertyTypeInteger, -75, 75),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                ESP_LOGI(TAG, "SetHeadAngle called with angle=%d", angle);
                target_head_pos = angle;
                return true;
            });

        mcp_server.AddTool("self.robot.move",
            "设置前进后退距离,单位厘米,正为前进,负为后退,不说具体值默认10厘米",
            PropertyList({
                Property("distance", kPropertyTypeInteger, -20, 20),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int distance = properties["distance"].value<int>();
                ESP_LOGI(TAG, "SetHeadAngle called with move distance=%d", distance);
                stable_pos = stable_pos + distance/100.0;
                return true;
            });

        mcp_server.AddTool("self.robot.rotate",
            "设置运动旋转角度,机身旋转注意和头部旋转区分!单位度,正为左,负为右,不说具体值默认30度",
            PropertyList({
                Property("angle", kPropertyTypeInteger, -180, 180),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                ESP_LOGI(TAG, "SetHeadAngle called with rotate angle=%d", angle);
                stable_yaw = stable_yaw + angle;
                yaw_ctrl_time = esp_timer_get_time()/1000.0;  // 下达命令才yaw闭环运动，平时yaw不闭环
                return true;
            });

        mcp_server.AddTool("self.status.battery",
            "查询当前电池电量,返回剩余电量百分比",
            PropertyList(std::vector<Property>{}), [this](const PropertyList& properties) -> ReturnValue {
                int level = 0;
                bool charging = false, discharging = false;
                if (GetBatteryLevel(level, charging, discharging)) {
                    return std::string("当前电池电量 ") + std::to_string(level) + "%"
                        + (charging ? ", 正在充电" : "");
                }
                return std::string("暂未读取到电池电压");
            });

    }

public:
    HoverBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        
        InitializeCamera();
        
        InitializeUart();
        InitializeController();

        // 立即读取一次电池电压，避免等待 60 秒才有电量数据
        ReadServoVoltage(3);

        InitializeBootButton();
        
        // IMU 初始化
        imu_init();

        // XGO 控制任务
        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                xgo_control();
            }
            vTaskDelete(NULL);
        }, "xgo_task", 4096, this, 12, &xgo_task_handle_, 0);

        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                xgo_rx();
                vTaskDelay(pdMS_TO_TICKS(3));
            }
            vTaskDelete(NULL);
        }, "xgo_rx_task", 4096, this, 12, &xgo_rx_task_handle_, 1);
        ESP_LOGI(TAG, "XGO control tasks created");

        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                imu_read_once();
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            vTaskDelete(NULL);
        }, "imu_task", 4096, this, 12, &imu_task_handle_, 1);
        ESP_LOGI(TAG, "IMU control tasks created");

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

        char* json_str = cJSON_PrintUnformatted(root);
        std::string result(json_str);
        cJSON_free(json_str);
        cJSON_Delete(root);

        return result;
    }

    virtual void OnStartup() override {

        display_->SetEmotion("launch");
        Application::GetInstance().PlaySound(STARTUP_SOUND);
        ESP_LOGI(TAG, "Boot animation: launch + engine startup sound");
    }

    virtual void OnInitializationComplete() override {
        // gpio_set_level(LASER_GPIO, 0);  // 初始化完成，关闭激光
        ESP_LOGI(TAG, "Initialization complete, laser off");

        // 播放开机音效
        auto& app = Application::GetInstance();
        app.PlaySound(STARTUP_SOUND);
        
        // 启动调试Web服务器（WiFi连接后可通过IP访问）
        if (WifiManager::GetInstance().IsConnected()) {
            hover_debug_server_start();
            std::string ip = WifiManager::GetInstance().GetIpAddress();
            ESP_LOGI(TAG, "Debug server started at http://%s", ip.c_str());
        }
    }

    virtual void OnWifiConfigStart() override {
        ESP_LOGI(TAG, "WiFi config start, reset to stand then keep sit");
    }

    virtual void OnWifiConfigEnd() override {
        // 配网结束，从坐姿起身并播放成功语音

        Application::GetInstance().PlaySound(Lang::Sounds::OGG_WIFI_SUCCESS());
        ESP_LOGI(TAG, "WiFi config end, sit reset (stand up) and play success audio");
        
        // 启动调试Web服务器
        if (!hover_debug_server_is_running() && WifiManager::GetInstance().IsConnected()) {
            hover_debug_server_start();
            std::string ip = WifiManager::GetInstance().GetIpAddress();
            ESP_LOGI(TAG, "Debug server started at http://%s", ip.c_str());
        }
    }

    // 从舵机 ID=3 的 PRESENT_VOLTAGE 寄存器读取电池电压
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

    // Hover 使用引擎声作为成功提示音
    virtual std::string_view GetSuccessSound() override {
        return THROTTLE_SOUND;
    }

    virtual std::string GetBoardDescription() override {
        return "一个双轮平衡小车形态机器人，搭载圆形 240x240 LCD 屏幕、ESP32-S3 MCU、8MB PSRAM、"
               "2路智能舵机、IMU 姿态传感器、GC0308 摄像头，支持光剑控制。"
               "2S 锂电池供电(6.6V-8.4V)。";
    }
};

DECLARE_BOARD(HoverBoard);
