#ifndef __MOTOR_H
#define __MOTOR_H

/* Hover 组态：云台 EM3（ID=3）+ 轮毂 KP4012。协议见 em3.h / kp4012.h */

#include <stdint.h>
#include <stddef.h>
#include <driver/uart.h>

void SendMotorCommand(uint8_t *pData, uint16_t size);
void WriteByte_P_V(uint8_t ID, short pos, short vel);
void ReadMotorState(uint8_t ID);
void ReadServoVoltage(uint8_t ID);
void ReadWheelState(uint8_t readID);
void sendWheelTor(short tor1, short tor2);
void motor_rx();

extern float servo_voltage;

#endif
