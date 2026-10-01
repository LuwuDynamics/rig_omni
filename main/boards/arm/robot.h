#ifndef __ROBOT_H
#define __ROBOT_H
#include "motor.h"
#include "ik.h"

#define FLASH_ZERO_POS_ADDR 0xFFF000
#define PI 3.14159

enum TeachState {
    TEACH_IDLE = 0,
    TEACH_ENTERING,
    TEACH_RECORDING,
    TEACH_EXITING,
    TEACH_REPLAYING
};

#pragma pack(push, 1)
struct TeachFrame {
    short pos[5];
};
#pragma pack(pop)

#define TEACH_SAMPLE_MS         100
#define TEACH_MAX_DURATION_MS   15000
#define TEACH_MAX_FRAMES        (TEACH_MAX_DURATION_MS / TEACH_SAMPLE_MS)
#define TEACH_SAMPLE_CYCLES     50
#define TEACH_PLAYBACK_TICKS_PER_FRAME 40

void InitZeroPos();
void WriteZeroPos();
bool ReadZeroPos();
void robot_control();
void set_action_loop_flag(uint8_t flag);

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
extern float angle1;
extern float angle2;
extern float angle3;
extern float angle4;
extern float angle5;
extern int control_mode;
extern uint8_t isIMUInit;
extern float q_head;

extern TeachState teach_state;
extern TeachFrame* teach_frames;
extern uint32_t teach_frame_count;
extern uint16_t teach_sample_counter;
extern uint8_t teach_read_servo_id;
extern uint16_t teach_enter_counter;
extern bool teach_has_recording;

void teach_enter();
void teach_exit();
void teach_cancel();
int teach_play();

void arm_ik_update();
void touch_wiggle_trigger();

void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len);

#endif
