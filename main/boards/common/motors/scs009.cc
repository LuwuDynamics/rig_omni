#include "scs009.h"
#include "scs_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static int16_t clamp_pos(int16_t pos) {
    if (pos < SCS009_POS_MIN) {
        return SCS009_POS_MIN;
    }
    if (pos > SCS009_POS_MAX) {
        return SCS009_POS_MAX;
    }
    return pos;
}

void scs009_enable(uint8_t id, uint8_t mode) {
    scs_write_byte(id, SCS009_TORQUE_ENABLE, mode);
}

void scs009_enable_all(uint8_t count, uint8_t mode) {
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int j = 0; j < 10; j++) {
        for (uint8_t i = 0; i < count; i++) {
            scs009_enable((uint8_t)(i + 1), mode);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void scs009_read_state(uint8_t id) {
    scs_read(id, SCS009_PRESENT_STATE, 6);
}

void scs009_read_voltage(uint8_t id) {
    scs_read(id, SCS009_PRESENT_VOLTAGE, 1);
}

void scs009_sync_write_pos_time_speed(uint8_t count, const int16_t *pos, int16_t vel) {
    if (pos == nullptr || count == 0 || count > 16) {
        return;
    }
    uint8_t ids[16];
    uint8_t payload[16 * 6];
    for (uint8_t i = 0; i < count; i++) {
        ids[i] = (uint8_t)(i + 1);
        int16_t p = clamp_pos(pos[i]);
        uint8_t *d = &payload[i * 6];
        scs_store_be16(&d[0], (uint16_t)p);
        d[2] = 0;
        d[3] = 0;
        scs_store_be16(&d[4], (uint16_t)vel);
    }
    scs_sync_write(SCS009_GOAL_POSITION, 6, count, ids, payload);
}

bool scs009_decode_state(const uint8_t *frame, uint8_t length_field,
                         int16_t *pos, int16_t *vel, int16_t *tor) {
    if (frame == nullptr || length_field < 8) {
        return false;
    }
    if (pos) {
        *pos = scs_load_be16(frame[length_field - 3], frame[length_field - 2]);
    }
    if (vel) {
        *vel = scs_load_be16(frame[length_field - 1], frame[length_field]);
    }
    if (tor) {
        *tor = scs_load_be16(frame[length_field + 1], frame[length_field + 2]);
    }
    return true;
}
