#ifndef __ROBOT_H
#define __ROBOT_H
#include "motor.h"

#define FLASH_ZERO_POS_ADDR 0xFFF000
#define PI 3.14159
#define TARS_GAIT_FREQ_HZ     1.5f
#define TARS_GAIT_AMP_MAX     60.0f

void InitZeroPos();
bool WriteZeroPos();
bool ReadZeroPos();
void robot_control();
void set_action_loop_flag(uint8_t flag);

extern float vx;
extern float vyaw;
extern uint16_t motor_speed;
extern int calibrate_mode;
extern uint8_t Action_ID;
extern uint8_t ACTION_DONE;
extern uint8_t actionLoop_FLAG;
extern int control_mode;
extern uint8_t isIMUInit;
extern float q_head;
extern char g_calib_error_msg[48];

void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);

#endif
