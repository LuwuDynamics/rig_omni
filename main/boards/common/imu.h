#ifndef _IMU_H_
#define _IMU_H_
#include "xgo.h"
// IMU (QMI8658C) 接口
void imu_init();
void imu_read_once();
void imu_deinit();
bool imu_is_initialized();

// 姿态数据
extern float roll;
extern float pitch;
extern float yaw;
extern float accel_x;
extern float accel_y;
extern float accel_z;
extern float pit_gyro;
extern float dq;
#endif
