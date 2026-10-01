#include "motor.h"
#include "robot.h"
#include "em3.h"
#include "kp4012.h"
#include "scs_bus.h"

float servo_voltage = 0.0;
static float K_V = 60.0f * 3.1413f / 1024.0f;

static void OnBusFrame(uint8_t id, const uint8_t *frame, uint8_t length_field) {
    switch (id) {
        case 1:
        case 11: {
            int16_t pos = 0, vel = 0;
            if (!kp4012_decode_state(frame, length_field, &pos, &vel)) {
                return;
            }
            float temp_spd = 4.5f * (float)vel * K_V;
            wheel1_vel = lp_vel * wheel1_vel + (1 - lp_vel) * temp_spd;
            int temp_pos = pos;
            if (temp_pos - last11Pos < -800) {
                wheel1_x += 1023 + temp_pos - last11Pos;
            } else if (temp_pos - last11Pos > 800) {
                wheel1_x += -1023 + temp_pos - last11Pos;
            } else {
                wheel1_x += temp_pos - last11Pos;
            }
            last11Pos = temp_pos;
            break;
        }
        case 2:
        case 21: {
            int16_t pos = 0, vel = 0;
            if (!kp4012_decode_state(frame, length_field, &pos, &vel)) {
                return;
            }
            float temp_spd = 4.5f * (float)(-vel) * K_V;
            wheel2_vel = lp_vel * wheel2_vel + (1 - lp_vel) * temp_spd;
            int temp_pos = pos;
            if (temp_pos - last21Pos < -800) {
                wheel2_x -= 1023 + temp_pos - last21Pos;
            } else if (temp_pos - last21Pos > 800) {
                wheel2_x -= -1023 + temp_pos - last21Pos;
            } else {
                wheel2_x -= temp_pos - last21Pos;
            }
            last21Pos = temp_pos;
            break;
        }
        case 3:
            if (scs_decode_voltage(frame, length_field, &servo_voltage)) {
                return;
            }
            {
                int16_t pos = 0;
                if (em3_decode_state(frame, length_field, &pos, nullptr, nullptr)) {
                    q_head = (1500.0f - pos) / 10.0f;
                }
            }
            break;
        default:
            break;
    }
}

void SendMotorCommand(uint8_t *pData, uint16_t size) {
    uart_write_bytes(UART_NUM_2, pData, size);
    uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(50));
}

void ReadMotorState(uint8_t ID) {
    em3_read_state(ID);
}

void motor_rx() {
    static bool registered = false;
    if (!registered) {
        scs_bus_on_frame(OnBusFrame);
        registered = true;
    }
    scs_bus_poll(0);
}

void WriteByte_P_V(uint8_t ID, short pos, short vel) {
    em3_write_pos_vel(ID, pos, vel);
}

void ReadServoVoltage(uint8_t ID) {
    em3_read_voltage(ID);
}

void ReadWheelState(uint8_t readID) {
    kp4012_read_state(readID);
}

void sendWheelTor(short tor1, short tor2) {
    const uint8_t ids[6] = {55, 1, 2, 11, 21, 66};
    const int16_t pos[6] = {0, 0, 0, 0, 0, 0};
    const int16_t torque[6] = {0, tor1, tor2, tor1, tor2, 0};
    kp4012_sync_write_pos_torque(6, ids, pos, torque);
}
