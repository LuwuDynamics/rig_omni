#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "audio_service.h"
#include "ble_remote_control.h"
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

#define TAG "PUPPY"

// 长按重置 NVS 的时间阈值（毫秒）
static constexpr int kLongPressResetMs = 3000;
static constexpr int kLongPressShowEmotionMs = 1000;  // 长按1秒后显示表情

class PuppyBoard : public WifiBoard {
private:
    Button boot_button_;
    emote::EmoteDisplay* display_ = nullptr;  // AAF动画显示
    Esp32Camera* camera_ = nullptr;  // 初始化为nullptr
    TaskHandle_t xgo_task_handle_ = nullptr;
    TaskHandle_t xgo_rx_task_handle_ = nullptr;
    int64_t button_press_start_time_ = 0;  // 按键按下时间戳
    esp_timer_handle_t long_press_timer_ = nullptr;  // 长按检测定时器
    bool nvs_reset_emotion_shown_ = false;  // 是否已显示 nvs_reset 表情

    void InitializeUart() {
        uart_config_t uart_cfg = {
            .baud_rate = 1000000,
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
        esp_rom_gpio_pad_select_gpio(LASER_GPIO);
        gpio_reset_pin(LASER_GPIO);
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << LASER_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
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
                auto board = static_cast<PuppyBoard*>(arg);
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
                    app.PlaySound(Lang::Sounds::OGG_SUCCESS);
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
            // 标定模式下不处理单击（三连击由 detect_triple_click 处理）
            if (calibrate_mode == 1) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });

        // 双击切换 AEC 开关
        boot_button_.OnDoubleClick([]() {
            auto& app = Application::GetInstance();
            if (app.GetAecMode() == kAecOff) {
                app.SetAecMode(kAecOnServerSide);
                app.PlaySound(Lang::Sounds::OGG_OPEN_AEC);
            } else {
                app.SetAecMode(kAecOff);
                app.PlaySound(Lang::Sounds::OGG_CLOSE_AEC);
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
        short mid_pos[] = {1500, 1500, 1500, 1500, 1500};
        if(mode==1 && calibrate_mode==0){
            //printf("Enter calibration mode\n");
            calibrate_mode = 1;
            for(int i=0;i<10;i++){
                SetMotorAngle(mid_pos, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
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
        switch (mode) {
            case GpioMode::Off:
                gpio_set_level(LASER_GPIO, 0);
                break;
            case GpioMode::On:
                gpio_set_level(LASER_GPIO, 1);
                break;
            case GpioMode::Toggle:
                gpio_set_level(LASER_GPIO, 0);
                ESP_LOGI(TAG, "Switch lighting modes");
                gpio_set_level(LASER_GPIO, 1);
                break;
        }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.dog.move",
            "机器狗移动(vx,vyaw,time),前后移动速度vx(前正后负,0停下)和转向速度vyaw(左转正值,右转负值,0停下),time为移动时间(毫秒),time=0时持续移动,否则移动time毫秒后停止,默认70%幅度",
            PropertyList({
                Property("dog_vx", kPropertyTypeInteger, -100, 100),
                Property("dog_vyaw", kPropertyTypeInteger, -100, 100),
                Property("time", kPropertyTypeInteger, 0, 10000),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int dog_vx = properties["dog_vx"].value<int>();
                int dog_vyaw = properties["dog_vyaw"].value<int>();
                int time = properties["time"].value<int>();
                SetDogSpeed(dog_vx, dog_vyaw, time);
                return true;
            });

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

        mcp_server.AddTool("self.dog.Wave",
            "执行打招呼动作 / Wave hello",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Wave_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Naughty",
            "执行撒娇动作 / Act cute",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Naughty_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Swing",
            "执行前后运动动作 / Swing back and forth",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Swing_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Lookup",
            "执行祈求/抬头动作 / Look up",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Lookup_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Rolling",
            "执行左右摇摆动作 / Roll side to side",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Rolling_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Angry",
            "执行懊悔/生气动作 / Show anger",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Angry_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Swimming",
            "执行游泳动作 / Swimming motion",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Swimming_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Pee",
            "执行撒尿动作 / Pee action",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Pee_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Stretch",
            "执行伸懒腰动作 / Stretch",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Stretch_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Bouncing",
            "执行上下蹲起动作 / Bounce up and down",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Bouncing_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Shaking",
            "执行摇头晃脑动作 / Shake head",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Shaking_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Sit",
            "执行坐下动作 / Sit down",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Sit_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Scratch",
            "执行挠痒动作 / Scratch",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Scratch_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Hug",
            "执行抱抱动作 / Hug",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Action_ID = Hug_ID;
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.Reset",
            "执行复位动作 / Reset to standing",
            PropertyList(std::vector<Property>{}),
            [this](const PropertyList& properties) -> ReturnValue {
                Clear_State(2);
                vTaskDelay(pdMS_TO_TICKS(1000));  // 动作间隔
                return true;
            });

        mcp_server.AddTool("self.dog.action_loop",
            "设定表演模式/动作循环,1为开始,0为停止",
            PropertyList({
                Property("flag", kPropertyTypeInteger, 0, 1),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int flag = properties["flag"].value<int>();
                set_action_loop_flag(flag);
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

        mcp_server.AddTool("self.dog.set_motor_angle",
            "即兴动作，设定左前，右前，左后，右后，腰部的五个角度，time为持续时间(毫秒)，前四个角度为0时，四条腿垂直，狗站立，为90时，四条腿水平，狗爬下，第五个角度一般为0，正值上半身向左转，负值上半身向右转。例如默认站立的动作为（40,40,40,40,0）,打招呼动作抬起右前腿(80,120,40,40,30)，如果直接让右前120度会倒，所以左前伸出，且腰向左扭，要让它更加动态可以延时150毫秒，然后多次发右前腿在100、120来回变动的指令。这套随机动作的指令一次最好不要超过15条，需要考虑动作之间延时，最好在100-500毫秒之间，尽可能科学准确地完成用户下达的即兴动作指令。",
            PropertyList({
                Property("angle1", kPropertyTypeInteger, -135.0, 135.0),
                Property("angle2", kPropertyTypeInteger, -135.0, 135.0),
                Property("angle3", kPropertyTypeInteger, -135.0, 135.0),
                Property("angle4", kPropertyTypeInteger, -135.0, 135.0),
                Property("angle5", kPropertyTypeInteger, -30.0, 30.0),
                Property("time", kPropertyTypeInteger, 0, 10000),
            }), [this](const PropertyList& properties) -> ReturnValue {
                float a1 = float(properties["angle1"].value<int>());
                float a2 = float(properties["angle2"].value<int>());
                float a3 = float(properties["angle3"].value<int>());
                float a4 = float(properties["angle4"].value<int>());
                float a5 = float(properties["angle5"].value<int>());
                int time = properties["time"].value<int>();
                SetAngle(a1, a2, a3, a4, a5, time);
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
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_ENTER_REMOTE);
                        
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
                        Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXIT_REMOTE);
                    }
                    return std::string("蓝牙遥控模式已关闭");
                }
            });
    }

public:
    PuppyBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        
        InitializeCamera();
        
        InitializeUart();
        InitializeLaser();
        gpio_set_level(LASER_GPIO, 1);  // 启动时点亮激光，初始化完成后关闭
        
        // 初始化舵机零点（不阻塞，标定检查在 CheckCalibration 中进行）
        InitZeroPos();
        
        // 注册舵机堵转检测回调
        SetMotorStallCallback([](uint8_t motor_id) {
            ESP_LOGW(TAG, "Motor %d stall event triggered!", motor_id);
            auto& app = Application::GetInstance();
            
            // 1. 显示痛苦表情
            auto display = Board::GetInstance().GetDisplay();
            if (display) {
                display->SetEmotion("sad");
            }
            
            // 2. 播放"好疼啊"语音（暂用 exclamation，后续添加 pain.ogg）
            app.PlaySound(Lang::Sounds::OGG_PAIN);
            
            // 3. 卸力堵转的舵机（延迟执行，让语音有机会播放）
            app.Schedule([motor_id]() {
                EnableMotor(motor_id, 0);  // 禁用舵机
                ESP_LOGI(TAG, "Motor %d disabled due to stall", motor_id);
                
                // 2秒后重新启用舵机
                vTaskDelay(pdMS_TO_TICKS(2000));
                EnableMotor(motor_id, 1);
                ESP_LOGI(TAG, "Motor %d re-enabled", motor_id);
                
                // 恢复表情
                auto display = Board::GetInstance().GetDisplay();
                if (display) {
                    display->SetEmotion("neutral");
                }
            });
        });
        EnableStallDetection(true);
        
        InitializeBootButton();
        
        // IMU 初始化
        imu_init();
        
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
        // 开机站立后执行伸懒腰动作 + silly 表情 + 汪汪叫
        Action_ID = Stretch_ID;
        display_->SetEmotion("launch");
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_WOOF);
        ESP_LOGI(TAG, "Boot animation: stretch + silly + woof");
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
        // BluFi 配网：先恢复站立，然后执行保持坐姿动作
        Clear_State(2);  // 强制重置到站立状态
        Action_ID = Keep_Sit_ID;
        ESP_LOGI(TAG, "WiFi config start, reset to stand then keep sit");
    }

    virtual void OnWifiConfigEnd() override {
        // 配网结束，从坐姿起身并播放成功语音
        Action_ID = Sit_Reset_ID;
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_WIFI_SUCCESS);
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
        audio.PlaySound(Lang::Sounds::OGG_CALIBRATION_ENTER);
        
        // 阻塞等待标定完成（用户三击按键退出标定模式）
        ESP_LOGI(TAG, "Waiting for calibration... (triple click to exit)");
        while (calibrate_mode == 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        ESP_LOGI(TAG, "Calibration completed!");
        
        // 标定完成，启用舵机
        for (int i = 0; i < MOTOR_NUM; i++) {
            motor[i].Load = 1;
        }
        
        // 播放退出标定语音
        audio.PlaySound(Lang::Sounds::OGG_CALIBRATION_EXIT);
        
        // 恢复正常表情
        if (display) {
            display->SetEmotion("neutral");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等待语音播放
    }
};

DECLARE_BOARD(PuppyBoard);
