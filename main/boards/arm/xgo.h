#ifndef __XGO_H
#define __XGO_H
#include <driver/uart.h>
#include <driver/gpio.h>
#include <stddef.h>
#include "ik.h"
#define FLASH_ZERO_POS_ADDR 0xFFF000
#define MOTOR_NUM 5
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

// ============================================================
// 示教模式 (Teach Mode)
// ============================================================
enum TeachState {
    TEACH_IDLE = 0,       // 空闲
    TEACH_ENTERING,       // 回到 stand 姿态（扭矩 ON）
    TEACH_RECORDING,      // 扭矩 OFF，记录用户手动拖动的轨迹
    TEACH_EXITING,        // 扭矩 ON，回到 stand 姿态（结束后不自动回放）
    TEACH_REPLAYING       // 回放记录的动作
};

// 示教帧：5 个舵机位置偏移（相对 ZeroPos，每帧 10 字节）
#pragma pack(push, 1)
struct TeachFrame {
    short pos[5];  // motor[0..4] 相对 ZeroPos 的偏移
};
#pragma pack(pop)

#define TEACH_SAMPLE_MS         100   // 录制采样间隔 100ms
#define TEACH_MAX_DURATION_MS   15000 // 最长示教 15s
#define TEACH_MAX_FRAMES        (TEACH_MAX_DURATION_MS / TEACH_SAMPLE_MS)  // 150 帧
// 控制周期约 2ms（XGO_TASK_INTERVAL_MS），100ms / 2ms = 50；录制侧仍按周期计数
#define TEACH_SAMPLE_CYCLES     50
// 回放：相邻关键帧之间固定 40 步线性插值（不依赖实际周期是否刚好 2ms）
#define TEACH_PLAYBACK_TICKS_PER_FRAME 40

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

// 示教模式全局变量
extern TeachState teach_state;
extern TeachFrame* teach_frames;
extern uint32_t teach_frame_count;
extern uint16_t teach_sample_counter;
extern uint8_t teach_read_servo_id;
extern uint16_t teach_enter_counter;
extern bool teach_has_recording;  // 本次上电是否已完成过示教录制

void teach_enter();
void teach_exit();
void teach_cancel();
int teach_play();  // 回放示教轨迹；未示教过返回 -1

void arm_ik_update();
void touch_wiggle_trigger();  // 触摸双击：pitch/roll 快速正弦摆动 1s，冷却 1s

// 空闲微动：记录最后一次 MCP 动作的时间戳，xgo_control 据此判断是否进入空闲微动

// BLE/XGO 风格串口协议入口（从BLE FFF2 收到的数据直接丢给这里）
void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);
void ReadServoVoltage(uint8_t ID);

#endif
