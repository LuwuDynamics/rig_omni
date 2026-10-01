#include "robot.h"
#include "lulu_ble.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "robot_action.h"
#include "tars_display.h"
#include "board.h"
#include "display.h"

static const char* TAG = "ROBOT";

uint16_t zero_buffer_default[MOTOR_NUM] = {430, 540, 492};
uint16_t motor_speed = 0;
uint8_t Action_ID = 0;
uint8_t actionLoop_FLAG = 0;
char g_calib_error_msg[48] = {0};

int calibrate_mode = 0;
int init_flag = 0;
float vx = 0.0;
float vyaw = 0.0;

int control_mode = 0;
uint8_t isIMUInit = 0;
float q_head = 0.0;

void set_action_loop_flag(uint8_t flag) {
    (void)flag;
    Action_ID = 0;
    actionLoop_FLAG = 0;
    vx = 0.0f;
    vyaw = 0.0f;
}

bool WriteZeroPos() {
    // 写入前校验：所有舵机都必须有有效的反馈位置（FbPos 在 100~900 之间，
    // 说明状态帧被成功解析过）。有舵机没有检测到就直接失败，由调用方
    // 走屏幕报警提示（红色圆环 + 感叹号 + 底部错误信息），避免把无效值写进 flash。
    uint32_t data[MOTOR_NUM];
    // 先收集所有未检测到的舵机，一次性全部列在提示里
    char missing[16] = {0};
    size_t used = 0;
    for (int i = 0; i < MOTOR_NUM; i++) {
        if (motor[i].FbPos < 100 || motor[i].FbPos > 900) {
            used += snprintf(missing + used, sizeof(missing) - used, "%s%d", used ? " " : "", i + 1);
        }
    }
    if (used > 0) {
        snprintf(g_calib_error_msg, sizeof(g_calib_error_msg), "SERVO %s NOT DETECTED", missing);
        printf("Servo%s not detected, skip zero position write\r\n", missing);
        return false;
    }
    for (int i = 0; i < MOTOR_NUM; i++) {
        data[i] = motor[i].FbPos;
        motor[i].ZeroPos = (short)data[i];
        printf("write zeropos [%d]: %" PRIu32 "\r\n", i, data[i]);
    }
    esp_err_t err = esp_flash_erase_region(NULL, FLASH_ZERO_POS_ADDR, 4096);
    if (err != ESP_OK) {
        snprintf(g_calib_error_msg, sizeof(g_calib_error_msg), "FLASH ERASE FAILED");
        printf("Failed to erase zero position flash region\r\n");
        return false;
    }
    err = esp_flash_write(NULL, data, FLASH_ZERO_POS_ADDR, sizeof(data));
    if (err != ESP_OK) {
        snprintf(g_calib_error_msg, sizeof(g_calib_error_msg), "FLASH WRITE FAILED");
        printf("Failed to write zero position to flash\r\n");
        return false;
    }
    return true;
}

bool ReadZeroPos() {
    uint32_t data[MOTOR_NUM] = {0};
    esp_err_t err = esp_flash_read(NULL, data, FLASH_ZERO_POS_ADDR, sizeof(data));
    for (int i = 0; i < MOTOR_NUM; i++) {
        printf("zeropos [%d]: %" PRIu32 "\r\n", i, data[i]);
    }
    if (err != ESP_OK) {
        for (int i = 0; i < MOTOR_NUM; i++) {
            motor[i].ZeroPos = zero_buffer_default[i];
        }
        return false;
    }
    for (int i = 0; i < MOTOR_NUM; i++) {
        if (data[i] < 100 || data[i] > 900) {
            return false;
        } else {
            motor[i].ZeroPos = data[i];
        }
    }
    return true;
}

void InitZeroPos() {
    bool res = ReadZeroPos();
    for (int i = 0; i < MOTOR_NUM; i++) {
        motor[i].ID = i + 1;
        motor[i].Load = 0;
        motor[i].ZeroPos = res ? motor[i].ZeroPos : zero_buffer_default[i];
    }
    if (res) {
        for (int i = 0; i < MOTOR_NUM; i++) {
            motor[i].Load = 1;
        }
        printf("Device calibrated, zero positions loaded from flash\n");
    } else {
        printf("Device not calibrated, entering calibration mode\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        calibrate_mode = 1;
        short mid_pos[] = {M_N / 2, M_N / 2, M_N / 2};
        for (int i = 0; i < 10; i++) {
            SetMotorPos(mid_pos, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        EnableAllMotor(0);
    }
    init_flag = 1;
}

void detect_triple_click() {
    static int click_count = 0;
    static uint32_t first_click_time = 0;
    static bool button_pressed = false;

    int level = gpio_get_level(GPIO_NUM_0);
    uint32_t current_time = esp_timer_get_time() / 1000;

    if (level == 0 && !button_pressed) {
        button_pressed = true;
        if (click_count == 0) {
            first_click_time = current_time;
            click_count = 1;
        } else {
            if (current_time - first_click_time <= 1000) {
                click_count++;
                if (click_count >= 3) {
                    auto display = Board::GetInstance().GetDisplay();
                    if (calibrate_mode == 0) {
                        ESP_LOGI("ROBOT", "Triple click: Enter calibration mode");
                        calibrate_mode = 1;
                        auto* tars_display = dynamic_cast<TarsDisplay*>(display);
                        if (tars_display) {
                            tars_display->HideCalibrationError();
                        }
                        if (display) {
                            display->SetEmotion("calibration");
                        }
                        short mid_pos[] = {M_N / 2, M_N / 2, M_N / 2};
                        for (int i = 0; i < 10; i++) {
                            SetMotorPos(mid_pos, 500);
                            vTaskDelay(pdMS_TO_TICKS(200));
                        }
                        EnableAllMotor(0);
                    } else {
                        ESP_LOGI("ROBOT", "Triple click: Exit calibration mode");
                        if (WriteZeroPos()) {
                            calibrate_mode = 0;
                            EnableAllMotor(1);
                            if (display) {
                                display->SetEmotion("neutral");
                            }
                        } else {
                            auto* tars_display = dynamic_cast<TarsDisplay*>(display);
                            if (tars_display) {
                                tars_display->ShowCalibrationError(g_calib_error_msg);
                            }
                        }
                    }
                    click_count = 0;
                    first_click_time = 0;
                }
            } else {
                click_count = 1;
                first_click_time = current_time;
            }
        }
    }

    if (level == 1 && button_pressed) {
        button_pressed = false;
    }

    if (click_count > 0 && (current_time - first_click_time) > 1000) {
        click_count = 0;
        first_click_time = 0;
    }
}

void robot_control() {
    if (init_flag == 0) {
        return;
    }
    static uint32_t counter = 0;
    static uint32_t counter2 = 0;
    static uint8_t read_id = 1;

    counter++;
    counter2++;

    if (Action_ID == reset_ID) {
        Reset();
    }

    if (calibrate_mode == 0) {
        tars_gait_update();
    }

    if (counter2 % 20 == 0) {
        detect_triple_click();
    }

    if (counter % 50 == 0) {
        ReadMotorState(read_id);
        counter = 0;
        read_id++;
        if (read_id > MOTOR_NUM) {
            read_id = 1;
        }

        static int voltage_counter = 0;
        if (++voltage_counter % 1200 == 0) {
            ReadServoVoltage(1);
        }
    }
}

static int from_order_range(uint8_t b, int min, int max) {
    return min + (int)b * (max - min) / 255;
}

void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len) {
    if (!data || len < 7) {
        return;
    }

    size_t i = 0;
    while (i + 1 < len && !(data[i] == 0x55 && data[i + 1] == 0x00)) {
        ++i;
    }
    if (i + 1 >= len) {
        return;
    }

    const uint8_t* frame = &data[i];
    size_t remaining = len - i;
    if (remaining < 7) {
        return;
    }

    uint8_t length = frame[2];
    if (length > remaining) {
        return;
    }

    uint8_t order = frame[3];
    const uint8_t* payload = &frame[4];
    size_t payload_len = length - 7;
    if (4 + payload_len + 3 > remaining) {
        return;
    }

    uint8_t checksum = frame[4 + payload_len];
    uint8_t tail0 = frame[4 + payload_len + 1];
    uint8_t tail1 = frame[4 + payload_len + 2];

    if (tail0 != 0x00 || tail1 != 0xAA) {
        return;
    }

    uint32_t sum = length + order;
    for (size_t j = 0; j < payload_len; ++j) {
        sum += payload[j];
    }
    sum &= 0xFF;
    if (checksum != (uint8_t)(0xFF - sum)) {
        return;
    }

    if (order == 0x02) {
        if (payload_len < 1) return;
        uint8_t addr = payload[0];
        ESP_LOGI("ROBOT_BLE", "Read command: addr=0x%02X", addr);

        uint8_t resp[32];
        size_t resp_len = 0;

        if (addr == 0x07) {
            const char* version = "T-1.0.0";
            size_t ver_len = strlen(version);
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8 + ver_len;
            resp[3] = 0x12;
            resp[4] = addr;
            memcpy(&resp[5], version, ver_len);
            uint32_t s = resp[2] + resp[3] + resp[4];
            for (size_t j = 0; j < ver_len; j++) s += resp[5 + j];
            resp[5 + ver_len] = (uint8_t)(0xFF - (s & 0xFF));
            resp[6 + ver_len] = 0x00;
            resp[7 + ver_len] = 0xAA;
            resp_len = 8 + ver_len;
        } else if (addr == 0x01) {
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 100;
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        } else {
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 0x00;
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        }

        if (resp_len > 0) {
            lulu_ble_send(resp, resp_len);
        }
        return;
    }

    if (order != 0x00 && order != 0x01) {
        return;
    }
    if (payload_len < 2) {
        return;
    }

    uint8_t addr = payload[0];
    uint8_t value = payload[1];

    switch (addr) {
    case 0x30: {
        int v = from_order_range(value, -60, 60);
        tars_set_vx(v);
        break;
    }
    case 0x32: {
        int w = from_order_range(value, -60, 60);
        tars_set_vyaw(w);
        break;
    }
    case 0x3E: {
        vx = 0.0f;
        vyaw = 0.0f;
        Action_ID = 0;
        control_mode = 0;
        break;
    }
    default:
        break;
    }
}
