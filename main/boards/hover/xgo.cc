#include "xgo.h"
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
#include "xgo_action.h"
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
float servo_voltage = 0.0;  // ID=3 舵机读取的 PRESENT_VOLTAGE (0.1V 精度)

int rx_index = 0;


float lqr_k[4] = {0.0, 0.0, 0.0, 0.0};
int robot_state = 0;

float lp_vel = 0.7;
void Host2SCS(uint8_t *DataL, uint8_t* DataH, int Data)
{
	*DataL = Data & 0xff;
	*DataH = Data>>8 ;	
}

short Host(uint16_t DataL, uint16_t DataH)
{
	short Data;
	Data = DataH;
	Data<<=8;
	Data |= DataL;
	return Data;
}

void SendMotorCommand(uint8_t *pData,uint16_t size)
{
	uart_write_bytes(UART_NUM_2,pData,size);
    uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(50));
}

void ReadMotorState(uint8_t ID){
	uint8_t bBuf[8];
	uint8_t CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x04;
	bBuf[4] = 0x02;
	bBuf[5] = 0x48;
	bBuf[6] = 0x06;
	CheckSum = ID + 0x04 + 0x02 + 0x48 + 0x06;
	bBuf[7] = ~CheckSum;
	SendMotorCommand(bBuf, 8);
}

uint8_t isIMUInit = 0;
uint8_t rxFlag = 0;
uint8_t rxLen = 0;
uint8_t rxDataLen = 0;
uint8_t id = 0;
short rxBuffer[30] = {0};
short tempShortData;
int tempPos = 0;
float tempSpd = 0.0;
float K_V = 60.0*3.1413/1024.0;
void xgo_rx(){
    uint8_t tempBuf[10];
    uint8_t res = 0; 
    uint8_t checkSum = 0;
    short POS_LOW_Byte = 0;
    short POS_HIGH_Byte = 0;
    short VEL_LOW_Byte = 0;
    short VEL_HIGH_Byte = 0;
    // 超时 0 = 非阻塞：有数据读一个，没有立即结束
    while(uart_read_bytes(UART_NUM_2, tempBuf, 1, 0) > 0){
        res = tempBuf[0];
        switch(rxFlag)
        {
            case 0:
			if(res == 0xFF)
				{rxFlag = 1;rxBuffer[0] = 0xFF;}
				break;
            case 1:
                if(res == 0xFF)
                    {rxFlag = 2;rxBuffer[1] = 0xFF;}
                else{
                    rxFlag = 0;
                }
                    
                break;
            case 2:
                    rxBuffer[2] = res;
                    rxFlag = 3;
                    break;
            case 3:
                if(res == 0x08||res == 0x0B||res == 0x03)
                    {					
                        rxFlag = 4; 
                        rxBuffer[3] = res;
                        rxLen = 0;
                        rxDataLen = res;
                    }
                else{
                    rxFlag = 0;
                }
                break;
            case 4:
                rxBuffer[4+rxLen] = res;
                rxLen++;
                if(rxLen==rxDataLen){				
                    checkSum = 0;
                    rxFlag = 0;
                    for(int i=0; i<1+rxDataLen; i++){
                        checkSum += rxBuffer[2+i];
                    }
                    checkSum = ~checkSum;
                    if(checkSum != rxBuffer[3+rxDataLen]){
                        continue;
                    }

                    switch(rxBuffer[2])
                    {			
                        case 1:
                        case 11:
                            POS_LOW_Byte =  rxBuffer[rxDataLen - 3];
                            POS_HIGH_Byte =  rxBuffer[rxDataLen - 2];
                            VEL_LOW_Byte =  rxBuffer[rxDataLen - 1];
                            VEL_HIGH_Byte =  rxBuffer[rxDataLen];

                            tempShortData = Host(VEL_LOW_Byte, VEL_HIGH_Byte);
                            tempSpd = (float)4.5*tempShortData*K_V;	// 新固件速度要乘*4.8*  //实际为5.7倍 旧电机可以乘0.9
                            wheel1_vel = lp_vel*wheel1_vel + (1-lp_vel)*tempSpd;
                            // ESP_LOGI("XGO", " vx1:  %f", wheel1_vel);  
                            tempPos = Host(POS_LOW_Byte, POS_HIGH_Byte);
                            if(tempPos-last11Pos<-800){
                                wheel1_x += 1023 + tempPos - last11Pos;
                            }else if(tempPos-last11Pos>800){
                                wheel1_x += -1023 + tempPos - last11Pos;
                            }else{
                                wheel1_x += tempPos-last11Pos;
                            }
                            last11Pos = tempPos;
                            break;
                        case 2:
                        case 21:
                            POS_LOW_Byte =  rxBuffer[rxDataLen - 3];
                            POS_HIGH_Byte =  rxBuffer[rxDataLen - 2];
                            VEL_LOW_Byte =  rxBuffer[rxDataLen - 1];
                            VEL_HIGH_Byte =  rxBuffer[rxDataLen];

                            tempShortData = -Host(VEL_LOW_Byte, VEL_HIGH_Byte);
                            tempSpd = (float)4.5*tempShortData*K_V; 
                            wheel2_vel = lp_vel*wheel2_vel + (1-lp_vel)*tempSpd;
                            // ESP_LOGI("XGO", " vx2:  %f", wheel2_vel); 
                            tempPos = Host(POS_LOW_Byte, POS_HIGH_Byte);
                            if(tempPos-last21Pos<-800){
                                wheel2_x -= 1023 + tempPos - last21Pos;
                            }else if(tempPos-last21Pos>800){
                                wheel2_x -= -1023 + tempPos - last21Pos;
                            }else{
                                wheel2_x -= tempPos-last21Pos;
                            }
                            last21Pos = tempPos;
                            break;
                        
                        case 3:
                            if (rxDataLen == 3) {
                                // PRESENT_VOLTAGE 响应: 1 字节电压值 (0.1V 精度)
                                servo_voltage = rxBuffer[5] * 0.1f;
                            } else {
                                // 舵机位置响应
                                POS_LOW_Byte =  rxBuffer[rxDataLen - 1];
                                POS_HIGH_Byte =  rxBuffer[rxDataLen];
                                tempPos = POS_LOW_Byte | (POS_HIGH_Byte << 8);
                                q_head = (1500.0 - tempPos)/10.0;
                            }
                            break;

                        default:
                            break;
                    }
                }
                break;
            default:
                rxFlag = 0;
                break;
        }		
    }
}

void detect_triple_click() {
    static int click_count = 0;           
    static uint32_t first_click_time = 0; 
    static bool button_pressed = false; 
    
    int level = gpio_get_level(GPIO_NUM_0);
    uint32_t current_time = esp_timer_get_time() / 1000;     

    if (level == 0 && !button_pressed) {
        button_pressed = true;        
        if (click_count == 0) {
            first_click_time = current_time;
            click_count = 1;
        } else {
            if (current_time - first_click_time <= 1000) {
                click_count++;
                
                if (click_count >= 3) {
                    auto display = Board::GetInstance().GetDisplay();
                    click_count = 0;
                    first_click_time = 0;
                }
            } else {
                click_count = 1;
                first_click_time = current_time;
            }
        }
    }
    
    if (level == 1 && button_pressed) {
        button_pressed = false;
    }
    
    if (click_count > 0 && (current_time - first_click_time) > 1000) {
        click_count = 0;
        first_click_time = 0;
    }
}



void WriteByte_P_V(uint8_t ID, short pos,short vel){
	uint8_t bBuf[11];
	uint8_t checkSum = 0x00;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x07;
	bBuf[4] = 0x03;
	bBuf[5] = 0x35;
	bBuf[6] = pos & 0xff;
	bBuf[7] = pos>>8;
	bBuf[8] = vel & 0xff;
	bBuf[9] = vel>>8;
	checkSum = ID + 0x07 +0x03 + bBuf[5] + bBuf[6] + bBuf[7] + bBuf[8] + bBuf[9];
	bBuf[10] = 0xff - checkSum;
	SendMotorCommand(bBuf, 11);
}


void ReadServoVoltage(uint8_t readID){
    uint8_t bBuf[8];
    uint8_t CheckSum = 0;
    bBuf[0] = 0xff;
    bBuf[1] = 0xff;
    bBuf[2] = readID;
    bBuf[3] = 0x04;
    bBuf[4] = 0x02;
    bBuf[5] = 0x40;      // PRESENT_VOLTAGE 寄存器
    bBuf[6] = 0x01;      // 读取 1 字节
    CheckSum = readID + 0x04 + 0x02 + 0x40 + 0x01;
    bBuf[7] = ~CheckSum;
    SendMotorCommand(bBuf, 8);
}

void ReadWheelState(uint8_t readID)
{
	uint8_t bBuf[8];
	uint8_t CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = readID;
	bBuf[3] = 0x04;
	bBuf[4] = 0x02;
	bBuf[5] = 0x24;
	bBuf[6] = 0x06;
	CheckSum = readID + 0x04 + 0x02 + 0x24 + 0x06;
	bBuf[7] = 0xff - CheckSum;
	SendMotorCommand(bBuf, 8);
}

void SetVelOpenloop(uint8_t ID){
	uint8_t bBuf[8];
	uint8_t CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x04;
	bBuf[4] = 0x03;
	bBuf[5] = 0x11;
	bBuf[6] = 0x00;
	CheckSum = ID + 0x04 + 0x03 + 0x11 + 0x00;
	bBuf[7] = 0xff - CheckSum;
	SendMotorCommand(bBuf, 8);
}

void WritePos_Sync_kp(uint8_t ID[], uint8_t IDN, float Position[], short Torque[])
{
	int i = 0;
	uint8_t check = 0;
    uint8_t buf_kp[100];
	buf_kp[0] = 0xFF;
	buf_kp[1] = 0xFF;
	buf_kp[2] = 0xFE;
	
	buf_kp[3] = 4+5*IDN;
	
	buf_kp[4] = 0x83;

	buf_kp[5] = 0x1E;
	buf_kp[6] = 0x04;
	for(i = 0;i<IDN;i++)
	{
		buf_kp[6+5*i+1] = ID[i];
		Host2SCS(buf_kp+6+5*i+2, buf_kp+6+5*i+3, (int)Position[i]);
		buf_kp[6+5*i+4] = Torque[i] &0xff;
		buf_kp[6+5*i+5] = Torque[i] >>8;
		
	}
	for (i = 2;i<7+5*IDN;i++)
	{
		check = check + buf_kp[i];
	}
	buf_kp[7+5*IDN] = ~check;
	SendMotorCommand(buf_kp, 8+5*IDN);
}

void sendWheelTor(short tor1, short tor2){
    float position[6];
    short torque[6];
    uint8_t tempID[6] = {55, 1, 2, 11, 21, 66};
    
    position[0] = 0;
    position[1] = 0;
    position[2] = 0;
    position[3] = 0;
    position[4] = 0;
    position[5] = 0;

    torque[0] = 0;
    torque[1] = tor1;
    torque[2] = tor2;
    torque[3] = tor1;   // ID 11: 同左轮
    torque[4] = tor2;   // ID 21: 同右轮
    torque[5] = 0;

    WritePos_Sync_kp(tempID, 6, position, torque);
}


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
    imu_zero = -5.0;

    lqr_k[0] = 1400.0f;
    lqr_k[1] = 6.8f;
    lqr_k[2] = 32.0f;
    lqr_k[3] = 1.6f;
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

    if (fallen_pose) {
        if (fall_since_us == 0) {
            fall_since_us = now_us;
        }
        upright_since_us = 0;
        if (now_us - fall_since_us >= kFallHoldUs) {
            robot_state = 0;
        }
    } else {
        fall_since_us = 0;
    }

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

    if(fabsf(wheel1_vel) > 350.0f||fabsf(wheel2_vel) > 350.0f){
        pick_up_count++;
        if(pick_up_count > 8){
            robot_state = 0;
        }
    }else{
        pick_up_count = 0;
    }

}

void xgo_control() { 
    float temp_u = 0.0;
    float lqr_x = 0.0;
    float lqr_vx = 0.0;
    float lqr_q = 0.0;
    float lqr_dq = 0.0;
    update_state();

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
        // 离散 LQR 全状态反馈: u = -K·x, Ts≈6ms (xgo_control 周期)
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

    WriteByte_P_V(3, short(1500.0 - target_head_pos*10.0), 1300);
    vTaskDelay(pdMS_TO_TICKS(2));

    if(isIMUInit == 1){
        sendWheelTor(short(tor1), short(tor2));
    }else{
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

// LULU BLE/XGO 风格串口协议解析入口
// data: 一次 GATT 写入的完整帧（APP 侧按 XGO 协议打包）
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
        ESP_LOGI("XGO_BLE", "Read command: addr=0x%02X", addr);
        
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
            ESP_LOGI("XGO_BLE", "Sending response: len=%d, data=%02X %02X %02X %02X %02X...", 
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

void CalIWeakenPID(PID *pstPid)
{
	pstPid->fpE=pstPid->fpDes-pstPid->fpFB;	
	if(pstPid->fpE*pstPid->fpPreE<0){
		pstPid->fpUi = 0;
		pstPid->fpSumE = 0.0*pstPid->fpSumE;
	}
	pstPid->fpSumE += pstPid->fpE;
	if(pstPid->fpEMax>0){
		pstPid->fpSumE = Clip(pstPid->fpSumE, -pstPid->fpEMax, pstPid->fpEMax);
	}
	

	pstPid->fpUi = Clip(pstPid->fpKi * pstPid->fpSumE                , -pstPid->fpIMax, pstPid->fpIMax);

	pstPid->fpUd = Clip(pstPid->fpKd * (pstPid->fpE - pstPid->fpPreE), -pstPid->fpDMax, pstPid->fpDMax);
	if (pstPid->fpE < pstPid->fpEMin && pstPid->fpE > -pstPid->fpEMin)
	{
		pstPid->fpSumE = 0.7*pstPid->fpSumE;
		pstPid->fpUp = Clip(pstPid->fpKp * pstPid->fpE , -pstPid->fpPMax, pstPid->fpPMax);	
	}
	else
	{
		pstPid->fpUp = Clip(pstPid->fpKp * pstPid->fpE , -pstPid->fpPMax, pstPid->fpPMax);
	}
	pstPid->fpPreE = pstPid->fpE;
	pstPid->fpU = pstPid->fpUp + pstPid->fpUi + pstPid->fpUd;
	pstPid->fpU = Clip(pstPid->fpU, -pstPid->fpUMax, pstPid->fpUMax);
}