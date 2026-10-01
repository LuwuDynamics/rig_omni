#include "kp4012.h"
#include "scs_bus.h"

void kp4012_read_state(uint8_t id) {
    scs_read(id, KP4012_PRESENT_STATE, 6);
}

void kp4012_sync_write_pos_torque(uint8_t id_count, const uint8_t *ids,
                                  const int16_t *pos, const int16_t *torque) {
    if (ids == nullptr || pos == nullptr || torque == nullptr || id_count == 0 || id_count > 16) {
        return;
    }
    uint8_t payload[16 * 4];
    for (uint8_t i = 0; i < id_count; i++) {
        uint8_t *d = &payload[i * 4];
        scs_store_le16(&d[0], (uint16_t)pos[i]);
        scs_store_le16(&d[2], (uint16_t)torque[i]);
    }
    scs_sync_write(KP4012_GOAL, 4, id_count, ids, payload);
}

bool kp4012_decode_state(const uint8_t *frame, uint8_t length_field,
                         int16_t *pos, int16_t *vel) {
    if (frame == nullptr || length_field < 8) {
        return false;
    }
    if (pos) {
        *pos = scs_load_le16(frame[length_field - 3], frame[length_field - 2]);
    }
    if (vel) {
        *vel = scs_load_le16(frame[length_field - 1], frame[length_field]);
    }
    return true;
}
