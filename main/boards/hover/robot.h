#ifndef __ROBOT_H
#define __ROBOT_H
#include "motor.h"
#include <stddef.h>

#define FLASH_ZERO_POS_ADDR 0xFFF000
#define PI 3.14159

typedef struct
{
    float fpDes;
    float fpFB;
    float fpKp;
    float fpKi;
    float fpKd;
    float fpE;
    float fpPreE;
    float fpSumE;
    float fpU;
    float fpUMax;
    float fpEMax;
    float fpEMin;
    float fpUp;
    float fpPMax;
    float fpUi;
    float fpIMax;
    float fpUd;
    float fpDMax;
} PID;

void robot_control();
float Clip(float fpValue, float fpMin, float fpMax);
void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);
void InitializeController();
void SetControlMode(int mode);
bool SetGimbalPitch(float angle, bool relative);

extern int control_mode;
extern float gimbal_pitch_target;
extern float gimbal_kp;
extern float gimbal_kd;
extern float gimbal_ff;
extern float vx;
extern float vyaw;
extern float q_head;
extern float target_head_pos;
extern float dq_u_max;
extern uint8_t isIMUInit;
extern float imu_zero;
extern float stable_pos;
extern float stable_yaw;
extern float k_yaw;
extern float kd_pit;
extern float lp_vel;
extern float yaw_ctrl_time;
extern PID pid_pos;
extern PID pid_vel;
extern PID pid_pit;
extern float lqr_k[4];

extern float wheel1_vel;
extern float wheel2_vel;
extern float wheel1_x;
extern float wheel2_x;
extern int last11Pos;
extern int last21Pos;
extern float tor1;
extern float tor2;
extern int rx_index;

#endif
