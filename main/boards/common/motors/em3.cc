#include "em3.h"
#include "scs_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void em3_enable(uint8_t id, uint8_t mode) {
    scs_write_byte(id, EM3_TORQUE_ENABLE, mode);
}

void em3_enable_all(uint8_t count, uint8_t mode) {
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int j = 0; j < 10; j++) {
        for (uint8_t i = 0; i < count; i++) {
            em3_enable((uint8_t)(i + 1), mode);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void em3_read_state(uint8_t id) {
    scs_read(id, EM3_PRESENT_STATE, 6);
}

void em3_read_voltage(uint8_t id) {
    scs_read(id, EM3_PRESENT_VOLTAGE, 1);
}

void em3_write_pos_vel(uint8_t id, int16_t pos, int16_t vel) {
    uint8_t data[4];
    scs_store_le16(&data[0], (uint16_t)pos);
    scs_store_le16(&data[2], (uint16_t)vel);
    scs_write_bytes(id, EM3_GOAL_POSITION, data, 4);
}

void em3_sync_write_pos_vel(uint8_t count, const int16_t *pos, int16_t vel) {
    if (pos == nullptr || count == 0 || count > 16) {
        return;
    }
    uint8_t ids[16];
    uint8_t payload[16 * 6];
    for (uint8_t i = 0; i < count; i++) {
        ids[i] = (uint8_t)(i + 1);
        uint8_t *p = &payload[i * 6];
        scs_store_le16(&p[0], (uint16_t)pos[i]);
        scs_store_le16(&p[2], (uint16_t)vel);
        p[4] = 0;
        p[5] = 0;
    }
    scs_sync_write(EM3_GOAL_POSITION, 6, count, ids, payload);
}

bool em3_decode_state(const uint8_t *frame, uint8_t length_field,
                      int16_t *pos, int16_t *vel, int16_t *tor) {
    if (frame == nullptr || length_field < 8) {
        return false;
    }
    if (pos) {
        *pos = scs_load_le16(frame[length_field - 1], frame[length_field]);
    }
    if (vel) {
        *vel = scs_load_le16(frame[length_field - 3], frame[length_field - 2]);
    }
    if (tor) {
        *tor = scs_load_le16(frame[length_field + 1], frame[length_field + 2]);
    }
    return true;
}
