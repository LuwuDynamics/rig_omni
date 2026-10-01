#include "robot.h"
#include "lulu_ble.h"
#include "imu.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "robot_action.h"
#include "application.h"
#include "board.h"
#include "display.h"

float ctrl_vx = 0.0;
float stable_vx = 0.0;
float vx = 0.0;
float vyaw = 0.0;

float q_head=0.0;
float target_head_pos = 0.0;

float tor1 = 0.0;
float tor2 = 0.0;

PID pid_pos = {0,  0, 0.0,   0.000,0,	0,	0,  0,	0,	1000,	100,	 0.0001,	0,	1000,	0,	1000,	0,	1000};
PID pid_vel  = {0,  0, 0.0,   0.000,0,	0,	0,  0,	0,	100,	100,	 0.01,	0,	100,	0,	100,	0,	100};
PID pid_pit = {0,  0, 6.0,    0,    0.0, 0,	0,  0,	0,	1000,	0,	0.01,	0,	1000,	0,	1000,	0,	100};

float dq_u = 0.0;
float kd_pit = 0.7;
float dq_u_max = 60.0;

int last11Pos = 512;
int last21Pos = 512;
float wheel1_vel = 0.0;
float wheel2_vel = 0.0;
float wheek_vx = 0.0;
float wheel1_x = 0.0;
float wheel2_x = 0.0;
float wheel_x = 0.0;

float imu_zero = 0.0;
int stable_flag= 0;
float stable_pos = 0.0;
long stable_time = 0;
float stable_yaw = 0.0;
float yaw_u = 0.0;
float k_yaw = 4.0; //3.0
float yaw_ctrl_time = 0.0;

int rx_index = 0;

float lqr_k[4] = {0.0, 0.0, 0.0, 0.0};
int robot_state = 0;
int control_mode = 0;  // 0=小车  1=云台
float gimbal_pitch_target = 0.0f;
float gimbal_kp = 36.0f;
float gimbal_kd = 3.6f;
float gimbal_ff = -150.0f;
static bool gimbal_pitch_latched = false;

float lp_vel = 0.7;

uint8_t isIMUInit = 0;

void InitializeController(){
    pid_pos.fpKp = 60.0;
    pid_pos.fpKi = 0.0;
    pid_pos.fpKd = 0.0;

    pid_vel.fpKp = 0.15;
    pid_vel.fpKi = 0.01;
    pid_vel.fpKd = 0.0;

    pid_pit.fpKp = 21.0;
    pid_pit.fpKi = 0.0;
    pid_pit.fpKd = 0.0;

    kd_pit = 2.3;
    imu_zero = -2.0;

    lqr_k[0] = 1300.0f;
    lqr_k[1] = 5.0f;
    lqr_k[2] = 25.0f;
    lqr_k[3] = 0.4f;
}

void SetControlMode(int mode) {
    control_mode = (mode == 1) ? 1 : 0;
    gimbal_pitch_latched = false;
    if (control_mode == 1) {
        gimbal_pitch_target = Clip(pitch, -45.0f, 45.0f);
        ESP_LOGW("ROBOT", "mode=gimbal, pitch_target=%.1f", gimbal_pitch_target);
    } else {
        robot_state = 0;  // 回小车先倒地，避免突然平衡冲出
        ESP_LOGW("ROBOT", "mode=car");
    }
}

bool SetGimbalPitch(float angle, bool relative) {
    if (control_mode != 1) {
        return false;
    }
    // 用户语义抬头为正，IMU 抬头为负，只反目标不反力矩
    const float cmd = -angle;
    float next = relative ? (gimbal_pitch_target + cmd) : cmd;
    gimbal_pitch_target = Clip(next, -45.0f, 45.0f);
    gimbal_pitch_latched = true;
    ESP_LOGW("ROBOT", "gimbal pitch=%.1f", gimbal_pitch_target);
    return true;
}

void update_state(){
    wheel_x = (wheel1_x+wheel2_x)/2.0/1024.0*3.1415*0.06;
    wheek_vx = (wheel1_vel + wheel2_vel)/ 2.0;
    if(fabsf(pitch) > 15.0f){
        stable_time = esp_timer_get_time()/1000.0;
    }
    if((esp_timer_get_time()/1000.0-stable_time>1000)&&stable_flag == 0){
        stable_flag = 1;
    }

    if(stable_flag == 0){
        stable_pos = wheel_x;

    }
    if((fabsf(stable_pos - wheel_x) > 0.5f || fabsf(pitch) > 30.0f) && stable_flag){
        stable_flag = 0;
    }

    if(esp_timer_get_time()/1000.0-yaw_ctrl_time>3000){
        stable_yaw = yaw - q_head;
    }

    // 姿态超出安全范围持续 → 摔倒；持续直立 → 恢复（累计时间，避免每周期刷新计时）
    static const int64_t kFallHoldUs = 500LL * 1000;
    static const int64_t kStandHoldUs = 1500LL * 1000;
    const bool fallen_pose = (fabsf(pitch) >= 60.0f || fabsf(roll) >= 40.0f);
    const bool upright_pose = (fabsf(pitch) < 7.0f && fabsf(roll) < 7.0f);
    static int64_t fall_since_us = 0;
    static int64_t upright_since_us = 0;
    static int64_t pick_up_count = 0;
    const int64_t now_us = esp_timer_get_time();

    // 开机默认倒地：IMU 校准完成前 pitch/roll 保持 0，不能据此进入平衡，
    // 否则倾斜开机校准一结束就会按大误差猛推轮子。
    if (!isIMUInit) {
        robot_state = 0;
        fall_since_us = 0;
        upright_since_us = 0;
    } else if (fallen_pose) {
        if (fall_since_us == 0) {
            fall_since_us = now_us;
        }
        upright_since_us = 0;
        if (now_us - fall_since_us >= kFallHoldUs) {
            robot_state = 0;
        }
    } else {
        fall_since_us = 0;

        if (upright_pose) {
            if (upright_since_us == 0) {
                upright_since_us = now_us;
            }
            if (now_us - upright_since_us >= kStandHoldUs) {
                robot_state = 1;
            }
        } else {
            upright_since_us = 0;
        }
    }

    if(fabsf(wheel1_vel) > 350.0f||fabsf(wheel2_vel) > 350.0f){
        pick_up_count++;
        if(pick_up_count > 8){
            robot_state = 0;
        }
    }else{
        pick_up_count = 0;
    }

}

void robot_control() {
    float temp_u = 0.0;
    float lqr_x = 0.0;
    float lqr_vx = 0.0;
    float lqr_q = 0.0;
    float lqr_dq = 0.0;
    update_state();

    if (control_mode == 1) {
        // 云台：右轮 PD 跟俯仰，左轮 8 档旋钮，头部照旧
        if (isIMUInit && !gimbal_pitch_latched) {
            gimbal_pitch_target = Clip(pitch, -45.0f, 45.0f);
            gimbal_pitch_latched = true;
        }
        if (isIMUInit) {
            const float des = Clip(gimbal_pitch_target, -45.0f, 45.0f);
            const float pd = gimbal_kp * (des - pitch) - gimbal_kd * dq;
            const float ff = gimbal_ff * sinf(pitch * PI / 180.0f);
            tor2 = Clip(pd + ff, -500.0f, 500.0f);
        } else {
            tor2 = 0;
        }

        const float step = 1024.0f / 8.0f;
        float pos = (float)last11Pos;
        float target = roundf(pos / step) * step;
        if (target >= 1024.0f) target -= 1024.0f;
        if (target < 0.0f) target += 1024.0f;
        float err = target - pos;
        if (err > 512.0f) err -= 1024.0f;
        if (err < -512.0f) err += 1024.0f;
        if (fabsf(err) < 6.0f) err = 0.0f;  // 到位死区，避免在档位上抖动
        tor1 = Clip(1.5f * err, -80.0f, 80.0f);
    } else {

    // pid_pos.fpDes = stable_pos;
    // pid_pos.fpFB = wheel_x;
    // CalIWeakenPID(&pid_pos);

    // pid_vel.fpDes = vx + pid_pos.fpU;
    // pid_vel.fpFB = wheek_vx;
    // CalIWeakenPID(&pid_vel);

    // pid_pit.fpDes = imu_zero*cos(q_head*PI/180.0)+ pid_vel.fpU;
    // pid_pit.fpFB = pitch;       //调节平衡角度
    // CalIWeakenPID(&pid_pit);

    // dq_u = -kd_pit*dq;
    // dq_u = Clip(dq_u, -dq_u_max, dq_u_max);
    // if(pid_pit.fpU<0&&pid_pit.fpU>-5){
    //     pid_pit.fpU = -5;
    // }else if(pid_pit.fpU>0&&pid_pit.fpU<5){
    //     pid_pit.fpU = 5;
    // }

    // if(pid_pit.fpU<-20){
    //     pid_pit.fpU = pid_pit.fpU -10;
    // }else if(pid_pit.fpU>20){
    //     pid_pit.fpU = pid_pit.fpU +10;
    // }
    // temp_u = pid_pit.fpU + dq_u;

        {
            // 离散 LQR 全状态反馈: u = -K·x, Ts≈6ms (robot_control 周期)
            // x1 位置误差(m)  x2 速度误差(m/s)  x3 俯仰误差(deg)  x4 俯仰角速度(deg/s)
            // pitch_ref = imu_zero·cos(head) 补偿头部姿态对平衡角的影响
            const float pitch_ref = imu_zero * cosf(q_head * PI / 180.0f);
            lqr_x  = wheel_x - stable_pos;
            lqr_vx = wheek_vx - vx;
            lqr_q  = pitch - pitch_ref;
            lqr_dq = dq;

            temp_u = -(lqr_k[0] * lqr_x
                      + lqr_k[1] * lqr_vx
                      + lqr_k[2] * lqr_q
                      + lqr_k[3] * lqr_dq);

            if (temp_u < -20.0f) {
                temp_u -= 10.0f;
            } else if (temp_u > 20.0f) {
                temp_u += 10.0f;
            }
            temp_u = Clip(temp_u, -pid_pit.fpUMax, pid_pit.fpUMax);
        }

        yaw_u = k_yaw*(yaw-q_head-stable_yaw);
        yaw_u = Clip(yaw_u, -80, 80);
        if(robot_state==0){
            tor1 = 0;
            tor2 = 0;
        }else{
            tor1 = -temp_u + yaw_u;
            tor2 = temp_u + yaw_u;
        }
    }

    WriteByte_P_V(3, short(1500.0 - target_head_pos*10.0), 1300);
    vTaskDelay(pdMS_TO_TICKS(2));

    if (control_mode == 1 || isIMUInit == 1) {
        sendWheelTor(short(tor1), short(tor2));
    } else {
        sendWheelTor(0, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    if(rx_index == 0){
        ReadWheelState(1);
    }else if(rx_index == 1){
        ReadWheelState(2);
    }else if(rx_index == 2){
        ReadWheelState(11);
    }else if(rx_index == 3){
        ReadWheelState(21);
    }else{
        ReadMotorState(3);
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    if(rx_index++>4){
        rx_index = 0;
    }

    // 每分钟读一次电池电压（约 7500 个周期）
    static int voltage_counter_hover = 0;
    if (++voltage_counter_hover % 7500 == 0) {
        ReadServoVoltage(3);
    }

}

// 将 0~255 映射到 [min, max]
static int from_order_range(uint8_t b, int min, int max) {
    return min + (int)b * (max - min) / 255;
}

// LULU BLE 遥控协议解析入口
// data: 一次 GATT 写入的完整帧（APP 侧按遥控协议打包）
void lulu_ble_on_rx_bytes(const uint8_t* data, size_t len) {
    if (!data || len < 7) {
        return;
    }

    // 查找帧头 55 00
    size_t i = 0;
    while (i + 1 < len && !(data[i] == 0x55 && data[i + 1] == 0x00)) {
        ++i;
    }
    if (i + 1 >= len) {
        return;
    }

    const uint8_t* frame = &data[i];
    size_t remaining = len - i;
    if (remaining < 7) {
        return;
    }

    uint8_t length = frame[2];
    if (length > remaining) {
        // 不完整帧，丢弃
        return;
    }

    uint8_t order = frame[3];
    const uint8_t* payload = &frame[4];
    size_t payload_len = length - 7;
    if (4 + payload_len + 3 > remaining) {
        return;
    }

    uint8_t checksum = frame[4 + payload_len];
    uint8_t tail0    = frame[4 + payload_len + 1];
    uint8_t tail1    = frame[4 + payload_len + 2];

    if (tail0 != 0x00 || tail1 != 0xAA) {
        return;
    }

    // 校验和：LENGTH + ORDER + PAYLOAD 所有字节
    uint32_t sum = length + order;
    for (size_t j = 0; j < payload_len; ++j) {
        sum += payload[j];
    }
    sum &= 0xFF;
    if (checksum != (uint8_t)(0xFF - sum)) {
        return;
    }

    // 处理读命令 (ORDER_READ = 0x02)
    if (order == 0x02) {
        if (payload_len < 1) return;
        uint8_t addr = payload[0];
        ESP_LOGI("ROBOT_BLE", "Read command: addr=0x%02X", addr);

        // 构建响应帧: 55 00 LENGTH READ_READBACK(0x12) ADDR DATA... CHECKSUM 00 AA
        uint8_t resp[32];
        size_t resp_len = 0;

        if (addr == 0x07) {
            // versionNumber: 返回版本字符串，如 "L-1.0.0"
            const char* version = "L-1.0.0";
            size_t ver_len = strlen(version);

            // 帧格式: 55 00 LENGTH ORDER ADDR DATA... CHECKSUM 00 AA
            // LENGTH = 整帧长度 = 2 + 1 + 1 + 1 + ver_len + 1 + 2 = 8 + ver_len
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8 + ver_len;  // LENGTH = 整帧长度
            resp[3] = 0x12;         // READ_READBACK
            resp[4] = addr;
            memcpy(&resp[5], version, ver_len);

            // 计算校验和
            uint32_t s = resp[2] + resp[3] + resp[4];
            for (size_t j = 0; j < ver_len; j++) s += resp[5 + j];
            resp[5 + ver_len] = (uint8_t)(0xFF - (s & 0xFF));
            resp[6 + ver_len] = 0x00;
            resp[7 + ver_len] = 0xAA;
            resp_len = 8 + ver_len;
        } else if (addr == 0x01) {
            // battery: 返回电池电量 (0-100)
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;    // LENGTH
            resp[3] = 0x12; // READ_READBACK
            resp[4] = addr;
            resp[5] = 100;  // 电池电量 100%
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        } else {
            // 其他地址返回 0
            resp[0] = 0x55;
            resp[1] = 0x00;
            resp[2] = 8;
            resp[3] = 0x12;
            resp[4] = addr;
            resp[5] = 0x00;
            uint32_t s = resp[2] + resp[3] + resp[4] + resp[5];
            resp[6] = (uint8_t)(0xFF - (s & 0xFF));
            resp[7] = 0x00;
            resp[8] = 0xAA;
            resp_len = 9;
        }

        if (resp_len > 0) {
            ESP_LOGI("ROBOT_BLE", "Sending response: len=%d, data=%02X %02X %02X %02X %02X...",
                     resp_len, resp[0], resp[1], resp[2], resp[3], resp[4]);
            lulu_ble_send(resp, resp_len);
        }
        return;
    }

    // 处理写命令 (0x00/0x01)
    if (order != 0x00 && order != 0x01) {
        return;
    }

    if (payload_len < 2) {
        return;
    }

    uint8_t addr  = payload[0];
    uint8_t value = payload[1];

    switch (addr) {
    case 0x30: { // speedVx: 前后速度
        int v = from_order_range(value, -100, 100);
        vx = (float)v;
        break;
    }
    case 0x32: { // speedVyaw: 原地转向速度
        int w = from_order_range(value, -100, 100);
        vyaw = (float)w;
        break;
    }
    default:
        // 其他地址暂不处理，保留给后续扩展
        break;
    }
}

float Clip(float fpValue, float fpMin, float fpMax)
{
	if(fpValue <= fpMin)
	{
		return fpMin;
	}
	else if(fpValue >= fpMax)
	{
		return fpMax;
	}
	else
	{
		return fpValue;
	}
}
