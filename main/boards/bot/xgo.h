#ifndef __XGO_H
#define __XGO_H
#include <driver/uart.h>
#include <driver/gpio.h>
#include <stddef.h>
#include "ik.h"
#define FLASH_ZERO_POS_ADDR 0xFFF000
#define MOTOR_NUM 21
#define PI 3.14159
#define M_N 1024
#define M_A 300.0
// 堵转检测阈值
#define STALL_POS_THRESHOLD   100   // 位置偏差阈值（DesPos 与 FbPos 的差值）
#define STALL_TOR_THRESHOLD   100   // 扭矩阈值（FbTor 绝对值）
#define STALL_DEBOUNCE_COUNT  3     // 防抖计数（连续N次超阈值才触发）
#define STALL_COOLDOWN_MS     1000  // 冷却时间（毫秒），防止频繁触发

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

// 堵转事件回调类型：参数为堵转的舵机ID (1-5)
typedef void (*motor_stall_callback_t)(uint8_t motor_id);

//Zero Position Functions
void InitZeroPos();
void WriteZeroPos();
bool ReadZeroPos();
bool IsCalibrated();  // 检查是否已标定
//Motor Control Functions
void EnableMotor(uint8_t ID, uint8_t mode);
void EnableAllMotor(int mode);
void SetMotorPos(short pos[],short vel);
void SetMotorAngle(float angle[],short vel);
void SendMotorCommand(uint8_t *pData, uint16_t size);
//Movement & Control Functions

void xgo_control();
void xgo_rx();
//Action & Behavior Functions
void set_action_loop_flag(uint8_t flag);

// 堵转检测
void EnableStallDetection(bool enable);

extern RigArmIK arm_ik;
extern float arm_angle[5];
extern float arm_x;
extern float arm_y;
extern float arm_z;
extern float arm_yaw;
extern float arm_pitch;
extern float arm_roll;
extern float arm_w;

extern float vx;
extern float vyaw;
extern uint16_t motor_speed;
extern int calibrate_mode;
extern uint8_t Action_ID;
extern uint8_t ACTION_DONE;
extern uint8_t actionLoop_FLAG;
extern Motor motor[MOTOR_NUM];
extern float angle1;
extern float angle2;
extern float angle3;
extern float angle4;
extern float angle5;
extern int control_mode;
extern uint8_t isIMUInit;
extern float q_head;
extern float servo_voltage;

// 空闲微动：记录最后一次 MCP 动作的时间戳，xgo_control 据此判断是否进入空闲微动

// BLE/XGO 风格串口协议入口（从BLE FFF2 收到的数据直接丢给这里）
void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);
void ReadServoVoltage(uint8_t ID);

#endif
