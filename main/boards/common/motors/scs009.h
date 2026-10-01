#ifndef RIG_SCS009_H
#define RIG_SCS009_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SCS009 串口舵机（飞特 SCS 帧，位置大端，行程 0–1023）
 * Arm、Tars 共用。从 0x2A 起写 6 字节：目标位置(2) + 时间(2, 固定 0) + 速度(2)。
 */
enum {
    SCS009_TORQUE_ENABLE   = 0x28,
    SCS009_GOAL_POSITION   = 0x2A,  /* 连续 6 字节：pos, time=0, speed */
    SCS009_PRESENT_STATE   = 0x38,
    SCS009_PRESENT_VOLTAGE = 0x3E,
    SCS009_POS_MIN         = 1,
    SCS009_POS_MAX         = 1023,
};

void scs009_sync_write_pos_time_speed(uint8_t count, const int16_t *pos, int16_t vel);

void scs009_enable(uint8_t id, uint8_t mode);
void scs009_enable_all(uint8_t count, uint8_t mode);
void scs009_read_state(uint8_t id);
void scs009_read_voltage(uint8_t id);
bool scs009_decode_state(const uint8_t *frame, uint8_t length_field,
                         int16_t *pos, int16_t *vel, int16_t *tor);

#ifdef __cplusplus
}
#endif

#endif
