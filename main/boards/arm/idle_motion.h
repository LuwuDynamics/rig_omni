#ifndef IDLE_MOTION_H
#define IDLE_MOTION_H

#include <stdbool.h>

// 初始化空闲微动状态（ArmBoard 构造时调用）
void idle_motion_init();

// 每控制周期调用一次，在 arm_ik_update() 之后、SetMotorAngle() 之前
// 给 arm_angle[0] (q0, base yaw) 和 arm_angle[4] (q4, wrist pitch) 叠加正弦偏移
void idle_motion_update();

// MCP 工具调用：开关空闲微动
void idle_motion_set_enable(bool enable);

// 查询状态
bool idle_motion_is_enabled();

#endif
