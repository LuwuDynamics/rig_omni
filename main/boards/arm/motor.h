#ifndef __MOTOR_H
#define __MOTOR_H

/* Arm 组态：5 × SCS009（写目标 pos-time-speed）。协议见 boards/common/motors/scs009.h */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <driver/uart.h>

#define MOTOR_NUM 5
#define M_N 1024
#define M_A 300.0
#define STALL_POS_THRESHOLD   100
#define STALL_TOR_THRESHOLD   100
#define STALL_DEBOUNCE_COUNT  3
#define STALL_COOLDOWN_MS     1000

typedef struct
{
    uint8_t ID;
    short DesPos;
    float DesSpd;
    short DesTor;
    short FbPos;
    float FbSpd;
    short FbTor;
    short ZeroPos;
    uint8_t Load;
} Motor;

typedef void (*motor_stall_callback_t)(uint8_t motor_id);

void SendMotorCommand(uint8_t *pData, uint16_t size);
void SetMotorPos(short pos[], short vel);
void SetMotorAngle(float angle[], short vel);
void EnableMotor(uint8_t ID, uint8_t mode);
void EnableAllMotor(int mode);
void ReadMotorState(uint8_t ID);
void ReadServoVoltage(uint8_t ID);
void EnableStallDetection(bool enable);
void motor_rx();

extern Motor motor[MOTOR_NUM];
extern float servo_voltage;

#endif
