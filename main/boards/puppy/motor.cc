#include "motor.h"
#include "em3.h"
#include "scs_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

Motor motor[MOTOR_NUM];
float servo_voltage = 0.0;

static void OnEm3Frame(uint8_t id, const uint8_t *frame, uint8_t length_field) {
    if (scs_decode_voltage(frame, length_field, &servo_voltage)) {
        return;
    }
    if (id < 1 || id > MOTOR_NUM) {
        return;
    }
    int16_t pos = 0, vel = 0, tor = 0;
    if (!em3_decode_state(frame, length_field, &pos, &vel, &tor)) {
        return;
    }
    uint8_t idx = (uint8_t)(id - 1);
    motor[idx].FbPos = pos;
    motor[idx].FbSpd = vel;
    motor[idx].FbTor = tor;
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

void SetMotorAngle(short angle[], short vel) {
    em3_sync_write_pos_vel(MOTOR_NUM, angle, vel);
}

void ReadMotorState(uint8_t ID) {
    em3_read_state(ID);
}

void ReadServoVoltage(uint8_t ID) {
    em3_read_voltage(ID);
}

void EnableMotor(uint8_t ID, uint8_t mode) {
    em3_enable(ID, mode);
}

void EnableAllMotor(int mode) {
    em3_enable_all(MOTOR_NUM, (uint8_t)mode);
}

void motor_rx() {
    static bool registered = false;
    if (!registered) {
        scs_bus_on_frame(OnEm3Frame);
        registered = true;
    }
    scs_bus_poll(5);
}
