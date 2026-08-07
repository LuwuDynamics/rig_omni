#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "audio_service.h"
#include "ble_remote_control.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "esp32_camera.h"
#include "ik.h"
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

#define TAG "BOT"

// 长按重置 NVS 的时间阈值（毫秒）
static constexpr int kLongPressResetMs = 3000;
static constexpr int kLongPressShowEmotionMs = 1000;  // 长按1秒后显示表情

// ARM 专属启动音效
extern const char arm_startup_ogg_start[] asm("_binary_arm_startup_ogg_start");
extern const char arm_startup_ogg_end[]   asm("_binary_arm_startup_ogg_end");
static const std::string_view ARM_STARTUP_SOUND {
    static_cast<const char*>(arm_startup_ogg_start),
    static_cast<size_t>(arm_startup_ogg_end - arm_startup_ogg_start)
};

class BotBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    emote::EmoteDisplay* display_ = nullptr;  // AAF动画显示
    Esp32Camera* camera_ = nullptr;  // 初始化为nullptr
    TaskHandle_t xgo_task_handle_ = nullptr;
    TaskHandle_t xgo_rx_task_handle_ = nullptr;
    int64_t button_press_start_time_ = 0;  // 按键按下时间戳
    esp_timer_handle_t long_press_timer_ = nullptr;  // 长按检测定时器
    bool nvs_reset_emotion_shown_ = false;  // 是否已显示 nvs_reset 表情

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
        uart_set_pin(UART_NUM_2, XGO_UART_TX_PIN, XGO_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
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
            .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
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

    void SetAngle(float a1, float a2, float a3, float a4, float a5, int t) {
        control_mode = 1;
        angle1 = a1;
        angle2 = a2;
        angle3 = a3;
        angle4 = a4;
        angle5 = a5;
        if (t > 0) {
            vTaskDelay(pdMS_TO_TICKS(t));
        }
    }

    void InitializeButtons() {
        // 创建长按检测定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto board = static_cast<BotBoard*>(arg);
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
                    app.PlaySound(Lang::Sounds::OGG_SUCCESS());
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

        // Touch 传感器 —— 单击进入/退出标定
        touch_button_.OnClick([this]() {
            if (calibrate_mode == 1) {
                ESP_LOGI(TAG, "Touch click detected, exiting calibration mode");
                Calibrate(0);
            }
        });

        // 双击切换 AEC 开关
        boot_button_.OnDoubleClick([]() {
            auto& app = Application::GetInstance();
            if (app.GetAecMode() == kAecOff) {
                app.SetAecMode(kAecOnServerSide);
                app.PlaySound(Lang::Sounds::OGG_OPEN_AEC());
            } else {
                app.SetAecMode(kAecOff);
                app.PlaySound(Lang::Sounds::OGG_CLOSE_AEC());
            }
        });
    }

    void SetDogSpeed(int dog_vx, int dog_vyaw, int time) {
        ESP_LOGI(TAG, "SetDogSpeed: vx=%d, vyaw=%d, time=%d", dog_vx, dog_vyaw, time);
        control_mode = 0;
        motor_speed = 0;
        vx = int(2.2 * dog_vx);
        vyaw = int(2.8 * dog_vyaw);
        if (time > 0) {
            vTaskDelay(pdMS_TO_TICKS(time));
        }
        vx = 0.0;
        vyaw = 0.0;
        ESP_LOGI(TAG, "SetDogSpeed: done");
    }

    void Calibrate(int mode) {
        short mid_pos[] = {M_N/2, M_N/2, M_N/2, M_N/2, M_N/2};
        if(mode==1 && calibrate_mode==0){
            //printf("Enter calibration mode\n");
            calibrate_mode = 1;
            for(int i=0;i<5;i++){
                SetMotorPos(mid_pos, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            EnableAllMotor(0);
            // Show calibration mode emotion
            if (display_) {
                display_->SetEmotion("calibration");
            }
        }
        if(mode==0 && calibrate_mode==1){
            WriteZeroPos();
            EnableAllMotor(1);
            calibrate_mode = 0;
            // Reset to normal emotion when exiting calibration mode
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


        mcp_server.AddTool("self.dog.calibrate",
            "标定机器狗,1为进入标定,0为退出/完成标定",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 1),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int mode = properties["mode"].value<int>();
                ESP_LOGI(TAG, "Calibrate called with mode=%d", mode);
                Calibrate(mode);
                return true;
            });

        mcp_server.AddTool("self.arm.node",
            "在和用户聊天时，让机械臂上下点头，表示肯定、同意或打招呼",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                node();
                return true;
            });

        mcp_server.AddTool("self.arm.shake",
            "在和用户聊天时，让机械臂左右摇头，表示否定、不同意或拒绝",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                shake();
                return true;
            });

        mcp_server.AddTool("self.arm.x",
            "机械臂头部前后移动，单位为厘米，正为向前，负为向后",
            PropertyList({
                Property("x", kPropertyTypeInteger, -3, 3),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int x = properties["x"].value<int>();
                arm_x = arm_x + x/100.0;
                if(arm_x >0.08){
                    arm_x = 0.08;
                }else if(arm_x <-0.08){
                    arm_x = -0.08;
                }
                return true;
            });

        mcp_server.AddTool("self.arm.y",
            "机械臂头部左右移动，单位为厘米，正为向左，负为向右",
            PropertyList({
                Property("y", kPropertyTypeInteger, -2, 2),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int y = properties["y"].value<int>();
                arm_y = arm_y + y/100.0;
                if(arm_x >0.05){
                    arm_x = 0.05;
                }else if(arm_x <-0.05){
                    arm_x = -0.05;
                }
                return true;
            });

        mcp_server.AddTool("self.arm.z",
            "机械臂头部上下移动，单位为厘米，正为向上，负为向下",
            PropertyList({
                Property("z", kPropertyTypeInteger, -5, 5),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int z = properties["z"].value<int>();
                arm_z = arm_z + z/100.0;
                if(arm_z >0.23){
                    arm_z = 0.23;
                }else if(arm_z <0.15){
                    arm_z = 0.15;
                }
                return true;
            });

        mcp_server.AddTool("self.arm.yaw",
            "机械臂头部左右旋转，单位为度，正为向左，负为向右",
            PropertyList({
                Property("yaw", kPropertyTypeInteger, -60   , 60),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int temp_yaw = properties["yaw"].value<int>();
                arm_yaw = arm_yaw + temp_yaw/180.0*PI;
                if(arm_yaw >90.0/180.0*PI){
                    arm_yaw = 90.0/180.0*PI;
                }else if(arm_yaw <-90.0/180.0*PI){
                    arm_yaw = -90.0/180.0*PI;
                }
                return true;
            });

        mcp_server.AddTool("self.arm.pitch",
            "机械臂头部上下旋转，单位为度，正为向下，负为向上",
            PropertyList({
                Property("pitch", kPropertyTypeInteger, -30, 30),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int temp_pitch = properties["pitch"].value<int>();
                arm_pitch = arm_pitch + temp_pitch/180.0*PI;
                if(arm_pitch >60.0/180.0*PI){
                    arm_pitch = 60.0/180.0*PI;
                }else if(arm_pitch <-60.0/180.0*PI){
                    arm_pitch = -60.0/180.0*PI;
                }
                return true;
            });

        mcp_server.AddTool("self.arm.roll",
            "机械臂头部左右歪头，单位为度，正为向左歪头，负为向右歪头，表示疑问或思考",
            PropertyList({
                Property("roll", kPropertyTypeInteger, -30, 30),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int temp_roll = properties["roll"].value<int>();
                arm_roll = arm_roll + temp_roll/180.0*PI;
                if(arm_roll >60.0/180.0*PI){
                    arm_roll = 60.0/180.0*PI;
                }else if(arm_roll <-60.0/180.0*PI){
                    arm_roll = -60.0/180.0*PI;
                }
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

       

        // BLE 遥控模式
        mcp_server.AddTool("self.ble.remote_control",
            "开启/关闭蓝牙遥控模式。开启后可用小程序或 APP 遥控机器狗（蓝牙名称与配网时相同）。"
            "enable=1 开启遥控模式, enable=0 关闭遥控模式",
            PropertyList({
                Property("enable", kPropertyTypeInteger, 0, 1),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int enable = properties["enable"].value<int>();
                if (enable) {
                    if (!ble_remote_is_running()) {
                        ESP_LOGI(TAG, "Starting BLE remote control mode");
                        // 显示遥控模式表情
                        if (display_) {
                            display_->SetEmotion("remote_mode");
                        }
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_ENTER_REMOTE());
                        
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
                        // 恢复表情并播放退出语音
                        if (display_) {
                            display_->SetEmotion("neutral");
                        }
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXIT_REMOTE());
                    }
                    return std::string("蓝牙遥控模式已关闭");
                }
            });
    }

public:
    BotBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        
        InitializeCamera();
        
        InitializeUart();
        InitializeLaser();
        gpio_set_level(LASER_GPIO, 1);  // 启动时点亮激光，初始化完成后关闭
        
        InitZeroPos();

        // 立即读取一次电池电压，避免等待 60 秒才有电量数据
        ReadServoVoltage(1);
        
        EnableStallDetection(true);
        
        InitializeBootButton();
        
        // IMU 初始化
        imu_init();
        rig_arm_ik_init(&arm_ik, 0.05);
        // XGO 控制任务
        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                xgo_control();
                vTaskDelay(pdMS_TO_TICKS(XGO_TASK_INTERVAL_MS));
            }
            vTaskDelete(NULL);
        }, "xgo_task", 4096, this, 5, &xgo_task_handle_, 0);

        xTaskCreatePinnedToCore([](void* arg) {
            (void)arg;
            while (true) {
                xgo_rx();
                imu_read_once();
                vTaskDelay(pdMS_TO_TICKS(XGO_RX_TASK_INTERVAL_MS));
            }
            vTaskDelete(NULL);
        }, "xgo_rx_task", 4096, this, 5, &xgo_rx_task_handle_, 1);
        ESP_LOGI(TAG, "XGO control tasks created");
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
        // 开机站立后执行伸懒腰动作 + launch 表情 + ARM 专属启动音效
        display_->SetEmotion("launch");
        Application::GetInstance().PlaySound(ARM_STARTUP_SOUND);
        ESP_LOGI(TAG, "Boot animation: stretch + launch + arm_startup");
    }

    virtual void OnInitializationComplete() override {
        gpio_set_level(LASER_GPIO, 0);  // 初始化完成，关闭激光
        ESP_LOGI(TAG, "Initialization complete, laser off");
    }

    void SetLaser(bool on) override {
        gpio_set_level(LASER_GPIO, on ? 1 : 0);
    }

    bool GetLaser() override {
        return gpio_get_level(LASER_GPIO) == 1;
    }

    virtual void OnWifiConfigStart() override {
        ESP_LOGI(TAG, "WiFi config start, reset to stand then keep sit");
    }

    virtual void OnWifiConfigEnd() override {
        // 配网结束，从坐姿起身并播放成功语音
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_WIFI_SUCCESS());
        ESP_LOGI(TAG, "WiFi config end, sit reset (stand up) and play success audio");
    }

    virtual void CheckCalibration(Display* display, AudioService& audio) override {
        // 检查是否需要标定
        if (calibrate_mode != 1) {
            ESP_LOGI(TAG, "Device already calibrated, skipping calibration");
            return;
        }
        
        ESP_LOGW(TAG, "Device needs calibration, entering calibration mode");
        
        // 显示标定表情
        if (display) {
            display->SetEmotion("calibration");
        }
        
        // 播放进入标定语音
        audio.PlaySound(Lang::Sounds::OGG_CALIBRATION_ENTER());
        
        // 阻塞等待标定完成（用户三击按键退出标定模式）
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
        audio.PlaySound(Lang::Sounds::OGG_CALIBRATION_EXIT());
        
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
        return "一个五自由度机械臂形态机器人，搭载圆形 240x240 LCD 屏幕、ESP32-S3 MCU、8MB PSRAM、"
               "5路智能舵机、IMU 姿态传感器、GC0308 摄像头，支持光剑控制。"
               "2S 锂电池供电(6.6V-8.4V)，通过舵机 ID=1 读取电压监测电量。";
    }

    // ARM 使用专属启动音效（arm_startup.ogg）
    virtual std::string_view GetSuccessSound() override {
        return ARM_STARTUP_SOUND;
    }
};

DECLARE_BOARD(BotBoard);
