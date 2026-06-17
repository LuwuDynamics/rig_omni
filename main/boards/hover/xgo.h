#ifndef __XGO_H
#define __XGO_H
#include <driver/uart.h>
#include <driver/gpio.h>
#include <stddef.h>

#define FLASH_ZERO_POS_ADDR 0xFFF000
#define PI 3.14159

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

typedef struct 
{
	float fpDes;  //控制变量目标值
	float fpFB;   //控制变量反馈值
	float fpKp;   //比例系数Kp
	float fpKi;   //积分系数Ki
	float fpKd;   //微分系数Kd
	float fpE;    //本次偏差
	float fpPreE; //上次偏差
  	float fpSumE; //总偏差
	float fpU;    //本次PID运算结果
	float fpUMax; //PID运算后输出最大值及做遇限削弱时的上限值
	float fpEMax; //做积分分离运算时偏差的最大值
	float fpEMin; //偏差死区
	float fpUp; //PID比例控制器的输出
	float fpPMax; //PID比例控制器的最大值
	float fpUi; //PID积分控制器输出
	float fpIMax; //PID积分控制器的最大值
	float fpUd; //PID微分控制器输出
	float fpDMax; //PID微分控制器的最大值
}PID;

void SendMotorCommand(uint8_t *pData, uint16_t size);
void move();
void xgo_control();
void xgo_rx();
void set_action_loop_flag(uint8_t flag);
void WriteByte_P_V(uint8_t ID, short pos,short vel);
float Clip(float fpValue, float fpMin, float fpMax);
void CalIWeakenPID(PID *pstPid);
void SetVelOpenloop(uint8_t ID);



void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);
void InitializeController();
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
extern PID pid_pos;
extern PID pid_vel;
extern PID pid_pit;
extern float lqr_k[4];
#endif
