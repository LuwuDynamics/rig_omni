#ifndef __ROBOT_H
#define __ROBOT_H
#include "motor.h"

#define FLASH_ZERO_POS_ADDR 0xFFF000
#define PI 3.14159

void InitZeroPos();
void WriteZeroPos();
bool ReadZeroPos();
void move();
void robot_control();
void set_action_loop_flag(uint8_t flag);

extern float vx;
extern float vyaw;
extern uint16_t motor_speed;
extern int calibrate_mode;
extern uint8_t Action_ID;
extern uint8_t ACTION_DONE;
extern uint8_t actionLoop_FLAG;
extern float angle1;
extern float angle2;
extern float angle3;
extern float angle4;
extern float angle5;
extern int control_mode;
extern uint8_t isIMUInit;
extern float q_head;

void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);

#endif
