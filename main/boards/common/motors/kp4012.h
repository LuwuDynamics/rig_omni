#ifndef RIG_KP4012_H
#define RIG_KP4012_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KP4012 轮毂电机（飞特 SCS 帧，小端）
 * Hover 左右轮使用。读状态地址与 EM3 舵机不同，不要混用 em3_read_state。
 */
enum {
    KP4012_GOAL          = 0x1E,  /* SYNC WRITE 每 ID 4 字节：pos(2) torque(2) */
    KP4012_PRESENT_STATE = 0x24,  /* 回读 6 字节 */
};

void kp4012_read_state(uint8_t id);
void kp4012_sync_write_pos_torque(uint8_t id_count, const uint8_t *ids,
                                  const int16_t *pos, const int16_t *torque);
bool kp4012_decode_state(const uint8_t *frame, uint8_t length_field,
                         int16_t *pos, int16_t *vel);

#ifdef __cplusplus
}
#endif

#endif
