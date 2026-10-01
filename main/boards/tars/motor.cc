#include "motor.h"
#include "robot.h"
#include "scs009.h"
#include "scs_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>

static const char *TAG = "MOTOR";

Motor motor[MOTOR_NUM];
static const int8_t kServoMask[MOTOR_NUM] = {1, 1, -1};
static motor_stall_callback_t stall_callback = nullptr;
static bool stall_detection_enabled = false;
static uint32_t last_stall_time = 0;
static uint8_t stall_count[MOTOR_NUM] = {0};
float servo_voltage = 0.0;

void EnableStallDetection(bool enable) {
    stall_detection_enabled = enable;
    if (!enable) {
        memset(stall_count, 0, sizeof(stall_count));
    }
}

static void CheckMotorStall(uint8_t id) {
    if (!stall_detection_enabled || !stall_callback || id < 1 || id > MOTOR_NUM) {
        return;
    }

    uint8_t idx = (uint8_t)(id - 1);
    int pos_err = abs(motor[idx].DesPos - motor[idx].FbPos);
    int tor = abs(motor[idx].FbTor);

    if (pos_err > STALL_POS_THRESHOLD && tor > STALL_TOR_THRESHOLD) {
        stall_count[idx]++;
        if (stall_count[idx] >= STALL_DEBOUNCE_COUNT) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_stall_time > STALL_COOLDOWN_MS) {
                ESP_LOGW(TAG, "Motor %d stall detected! pos_err=%d, tor=%d", id, pos_err, tor);
                last_stall_time = now;
                stall_count[idx] = 0;
                stall_callback(id);
            }
        }
    } else {
        stall_count[idx] = 0;
    }
}

static void OnScs009Frame(uint8_t id, const uint8_t *frame, uint8_t length_field) {
    if (scs_decode_voltage(frame, length_field, &servo_voltage)) {
        return;
    }
    if (id < 1 || id > MOTOR_NUM) {
        return;
    }
    int16_t pos = 0, vel = 0, tor = 0;
    if (!scs009_decode_state(frame, length_field, &pos, &vel, &tor)) {
        return;
    }
    uint8_t idx = (uint8_t)(id - 1);
    motor[idx].FbPos = pos;
    motor[idx].FbSpd = vel;
    motor[idx].FbTor = tor;
    CheckMotorStall(id);
}

void SendMotorCommand(uint8_t *pData, uint16_t size) {
    if (serial_lock) {
        return;
    }
    serial_lock = 1;
    uart_write_bytes(UART_NUM_2, pData, size);
    uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(50));
    serial_lock = 0;
}

void SetMotorPos(short pos[], short vel) {
    short clamped[MOTOR_NUM];
    for (int i = 0; i < MOTOR_NUM; i++) {
        short p = pos[i];
        if (p < 1) p = 1;
        if (p > 1023) p = 1023;
        motor[i].DesPos = p;
        clamped[i] = p;
    }
    scs009_sync_write_pos_time_speed(MOTOR_NUM, clamped, vel);
}

void SetMotorAngle(float angle[], short vel) {
    short pos[MOTOR_NUM];
    for (int i = 0; i < MOTOR_NUM; i++) {
        pos[i] = (short)(kServoMask[i] * angle[i] / M_A * M_N + motor[i].ZeroPos);
    }
    SetMotorPos(pos, vel);
}

void WritePos(const float pos[MOTOR_NUM]) {
    short poses[MOTOR_NUM];
    for (int i = 0; i < MOTOR_NUM; i++) {
        poses[i] = (short)(kServoMask[i] * pos[i] + motor[i].ZeroPos);
    }
    SetMotorPos(poses, motor_speed);
}

void ReadMotorState(uint8_t ID) {
    scs009_read_state(ID);
}

void ReadServoVoltage(uint8_t ID) {
    scs009_read_voltage(ID);
}

void EnableMotor(uint8_t ID, uint8_t mode) {
    scs009_enable(ID, mode);
}

void EnableAllMotor(int mode) {
    scs009_enable_all(MOTOR_NUM, (uint8_t)mode);
}

void motor_rx() {
    static bool registered = false;
    if (!registered) {
        scs_bus_on_frame(OnScs009Frame);
        registered = true;
    }
    scs_bus_poll(5);
}
