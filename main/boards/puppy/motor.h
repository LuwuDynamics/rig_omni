#ifndef __MOTOR_H
#define __MOTOR_H

/* Puppy 组态：5 × EM3。协议实现见 boards/common/motors/em3.h */

#include <stdint.h>
#include <stddef.h>
#include <driver/uart.h>

#define MOTOR_NUM 5

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

void SendMotorCommand(uint8_t *pData, uint16_t size);
void SetMotorAngle(short angle[], short vel);
void EnableMotor(uint8_t ID, uint8_t mode);
void EnableAllMotor(int mode);
void ReadMotorState(uint8_t ID);
void ReadServoVoltage(uint8_t ID);
void motor_rx();

extern Motor motor[MOTOR_NUM];
extern float servo_voltage;

#endif
