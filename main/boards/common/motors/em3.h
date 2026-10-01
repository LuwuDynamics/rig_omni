#ifndef RIG_EM3_H
#define RIG_EM3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * EM3 串口舵机（飞特 SCS 帧，小端）
 * Puppy 五腿、Hover 云台（ID=3）使用本驱动。
 */
enum {
    EM3_TORQUE_ENABLE   = 0x30,
    EM3_GOAL_POSITION   = 0x35,  /* 连续 6 字节：pos(2) vel(2) reserved(2)，小端 */
    EM3_PRESENT_VOLTAGE = 0x40,
    EM3_PRESENT_STATE   = 0x48,  /* 回读 6 字节 */
};

void em3_enable(uint8_t id, uint8_t mode);
void em3_enable_all(uint8_t count, uint8_t mode);
void em3_read_state(uint8_t id);
void em3_read_voltage(uint8_t id);
void em3_write_pos_vel(uint8_t id, int16_t pos, int16_t vel);
/* IDs 固定为 1..count，广播 SYNC WRITE。 */
void em3_sync_write_pos_vel(uint8_t count, const int16_t *pos, int16_t vel);
bool em3_decode_state(const uint8_t *frame, uint8_t length_field,
                      int16_t *pos, int16_t *vel, int16_t *tor);

#ifdef __cplusplus
}
#endif

#endif
