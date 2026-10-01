#ifndef RIG_MOTORS_H
#define RIG_MOTORS_H

/*
 * 电机驱动库（按 IC 型号拆分，不是按机器人拆分）
 *
 * 新机器人复用现有电机：在板级 motor.cc 里 include 对应头文件，不要复制寄存器数字。
 * 新电机型号：在本目录新增 <型号>.h/.cc（仿照 em3 / scs009 / kp4012），CMake 会自动编译 *.cc。
 *
 * | 型号    | 头文件     | 当前用户              |
 * |---------|------------|-----------------------|
 * | EM3     | em3.h      | Puppy 腿、Hover 云台  |
 * | SCS009  | scs009.h   | Arm、Tars            |
 * | KP4012  | kp4012.h   | Hover 轮毂            |
 *
 * 总线帧格式（飞特 SCS 兼容）在 scs_bus.h，三种电机共用。
 */

#include "scs_bus.h"
#include "em3.h"
#include "scs009.h"
#include "kp4012.h"

#endif
