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

static const char* TAG = "XGO";

Motor motor[MOTOR_NUM];
uint16_t zero_buffer[MOTOR_NUM] = {2400,600,2400,600,1500};
uint16_t zero_buffer_default[MOTOR_NUM] = {2400,600,2400,600,1500};
uint16_t motor_speed = 0;
uint8_t Action_ID = 0;
uint8_t actionLoop_FLAG = 0;
uint8_t serial_lock = 0;
float vx = 0.0;
float vyaw = 0.0;
int calibrate_mode = 0;
int init_flag = 0;
int control_mode = 0; //0为移动模式，1为直接控制角度模式
uint8_t isIMUInit = 0;
float q_head = 0.0;
float l_p[][5] = {{3*PI/4.0, 3*PI/4.0, 3*PI/4.0, 3*PI/4.0, PI/4.0},
                  {-PI/4.0, -PI/4.0, -PI/4.0, -PI/4.0, PI/4.0},
                  {-PI/4.0, 3*PI/4.0,  3*PI/4.0, -PI/4.0, PI/4.0},
                  {3*PI/4.0, -PI/4.0,  -PI/4.0, 3*PI/4.0, PI/4.0}};
float angle1 = 0.0;
float angle2 = 0.0;
float angle3 = 0.0;
float angle4 = 0.0;
float angle5 = 0.0;

// 堵转检测相关变量
static motor_stall_callback_t stall_callback = nullptr;
static bool stall_detection_enabled = false;
static uint32_t last_stall_time = 0;  // 上次触发堵转的时间
static uint8_t stall_count[MOTOR_NUM] = {0};  // 各舵机防抖计数

void SetMotorStallCallback(motor_stall_callback_t callback) {
    stall_callback = callback;
}

void EnableStallDetection(bool enable) {
    stall_detection_enabled = enable;
    if (!enable) {
        memset(stall_count, 0, sizeof(stall_count));
    }
}

// 通过位置偏差和扭矩检测堵转
static void CheckMotorStall(uint8_t id) {
    if (!stall_detection_enabled || !stall_callback || id < 1 || id > MOTOR_NUM) {
        return;
    }
    
    uint8_t idx = id - 1;
    int pos_err = abs(motor[idx].DesPos - motor[idx].FbPos);
    int tor = abs(motor[idx].FbTor);
    
    if (pos_err > STALL_POS_THRESHOLD && tor > STALL_TOR_THRESHOLD) {
        stall_count[idx]++;
        if (stall_count[idx] >= STALL_DEBOUNCE_COUNT) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_stall_time > STALL_COOLDOWN_MS) {
                ESP_LOGW(TAG, "Motor %d stall detected! pos_err=%d, tor=%d", id, pos_err, tor);
                last_stall_time = now;
                stall_count[idx] = 0;
                stall_callback(id);
            }
        }
    } else {
        stall_count[idx] = 0;
    }
}

void set_action_loop_flag(uint8_t flag){
    if(flag==1){
        Action_ID = 1;
        actionLoop_FLAG = 1;
    }else{
        Action_ID = 255;
        actionLoop_FLAG = 0;
    }
}

void WriteZeroPos(){
    uint32_t data[MOTOR_NUM];
    for(int i=0;i<MOTOR_NUM;i++){
        data[i] = motor[i].FbPos;
        motor[i].ZeroPos = motor[i].FbPos;
        printf("write zeropos [%d]: %d\r\n", i, motor[i].FbPos);
    }
    esp_err_t err = esp_flash_erase_region(NULL, FLASH_ZERO_POS_ADDR, 4096);
    if (err != ESP_OK) {
        printf("Failed to erase zero position flash region");
        return;
    }
    err = esp_flash_write(NULL, data, FLASH_ZERO_POS_ADDR, sizeof(data));
    if (err != ESP_OK) {
        printf("Failed to write zero position to flash");
        return;
    }
}

bool ReadZeroPos(){
    uint32_t data[MOTOR_NUM] = {0};    
    esp_err_t err = esp_flash_read(NULL, data, FLASH_ZERO_POS_ADDR, sizeof(data));
    for(int i=0;i<MOTOR_NUM;i++){
        printf("zeropos [%d]: %ld\r\n", i, data[i]);
    }
    if (err != ESP_OK) {
        for(int i=0;i<MOTOR_NUM;i++){
            motor[i].ZeroPos = zero_buffer_default[i];
        }
        return false;
    }
    for(int i=0;i<MOTOR_NUM;i++){  
        if(data[i]<200||data[i]>2800){
            return false;
        }else{
            motor[i].ZeroPos = data[i];
        }
    }
    return true;
}

void InitZeroPos(){
    bool res;
    res = ReadZeroPos();
    for(int i=0;i<MOTOR_NUM;i++){
        motor[i].ID = i+1;
        motor[i].Load = 0;
    }
    if(res){
        // 已标定，启用舵机
        for(int i=0;i<MOTOR_NUM;i++){
            motor[i].Load = 1;
        }
    }else{
        // 未标定，让舵机回到中位并禁用
        printf("Device not calibrated, entering calibration mode\n");
        short mid_pos[] = {1500, 1500, 1500, 1500, 1500};
        vTaskDelay(pdMS_TO_TICKS(500));
        calibrate_mode = 1;
        for(int i=0; i<10; i++){
            SetMotorAngle(mid_pos, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        EnableAllMotor(0);
        // 不阻塞，标定等待在 CheckCalibration 中进行
    }
    init_flag = 1;
}

void SendMotorCommand(uint8_t *pData,uint16_t size)
{
    if(serial_lock){
		return;
	}else{
		serial_lock = 1;
	}
	uart_write_bytes(UART_NUM_2,pData,size);
    uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(50));
	serial_lock = 0;
}

void SetMotorPos(uint8_t ID,uint8_t addr,short pos,short vel){
	uint8_t bBuf[11];
	uint8_t checkSum = 0x00;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x07;
	bBuf[4] = 0x03;
	bBuf[5] = addr;
	bBuf[6] = pos & 0xff;
	bBuf[7] = pos>>8;
	bBuf[8] = vel & 0xff;
	bBuf[9] = vel>>8;
	checkSum = ID + 0x07 +0x03 + addr + bBuf[6] + bBuf[7] + bBuf[8] + bBuf[9];
	bBuf[10] = 0xff - checkSum;
	SendMotorCommand(bBuf, 11);
}


void SetMotorAngle(short angle[],short vel){
    uint8_t bBuf[50];
    uint8_t checkSum = 0x00;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = 0xFE;
	bBuf[3] = (6+1)*5+4;  //length
	bBuf[4] = 0x83;
    bBuf[5] = 0x35;
    bBuf[6] = 0x06;
    for(int i=0;i<5;i++){
        bBuf[7+i*7] = i+1;
        bBuf[7+i*7+1] = angle[i] & 0xff;
        bBuf[7+i*7+2] = angle[i] >> 8;
        bBuf[7+i*7+3] = vel & 0xff;
        bBuf[7+i*7+4] = vel >> 8;
        bBuf[7+i*7+5] = 0;
        bBuf[7+i*7+6] = 0;
    }
    for(int i=0;i<39;i++){
        checkSum += bBuf[2+i];
    }
    bBuf[42] = 0xff - checkSum;
    SendMotorCommand(bBuf, 43);
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

void EnableMotor(uint8_t ID, uint8_t mode){
	uint8_t bBuf[8];
    uint8_t CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x04;
	bBuf[4] = 0x03;
	bBuf[5] = 0x30;
	bBuf[6] = mode;
	CheckSum = ID + 0x04 + 0x03 + 0x30 + mode;
	bBuf[7] = ~CheckSum;
	SendMotorCommand(bBuf, 8);
}

void EnableAllMotor(int mode){ 
    vTaskDelay(pdMS_TO_TICKS(100));
    for(int j=0;j<10;j++){
        for(int i=0;i<MOTOR_NUM;i++){
            EnableMotor(i+1, mode);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void move(){
    float ratio = 0.0;
    float step = 0.0;
    static float pace_t = 0.0;
    int x_index = 0;
    int yaw_index = 2;
    if(pace_t > 2.0*PI){
        pace_t = 0.0;
    }
    // pace_t += 0.077;
    pace_t += 0.085;
    step = sqrt(vx*vx+vyaw*vyaw);
            
    if(vx>0){
        x_index = 0;
    }else{
        x_index = 1;
    }
    if(vyaw>0){
        yaw_index = 3;
    }else{
        yaw_index = 2;
    }

    if(control_mode == 0){
        if(abs(vx)>15 || abs(vyaw)>15){
            ratio = abs(vx)/(abs(vx) + abs(vyaw));
            motor[0].DesPos = motor[0].ZeroPos - 700 + (short)(step*cos(pace_t + ratio*l_p[x_index][0] + (1-ratio)*l_p[yaw_index][0])); 
            motor[1].DesPos = motor[1].ZeroPos + 700 + (short)(step*cos(pace_t + ratio*l_p[x_index][1] + (1-ratio)*l_p[yaw_index][1])); 
            motor[2].DesPos = motor[2].ZeroPos - 700 - (short)(step*cos(pace_t + ratio*l_p[x_index][2] + (1-ratio)*l_p[yaw_index][2]));
            motor[3].DesPos = motor[3].ZeroPos + 700 - (short)(step*cos(pace_t + ratio*l_p[x_index][3] + (1-ratio)*l_p[yaw_index][3]));
            motor[4].DesPos = motor[4].ZeroPos + (short)(200.0*cos(pace_t + ratio*l_p[x_index][4] + (1-ratio)*l_p[yaw_index][4]));
            // motor[4].DesPos = motor[4].ZeroPos + (short)(step*1.5*cos(pace_t + ratio*l_p[x_index][4] + (1-ratio)*l_p[yaw_index][4]));
        }else{
            motor[0].DesPos = motor[0].ZeroPos - 550; 
            motor[1].DesPos = motor[1].ZeroPos + 550; 
            motor[2].DesPos = motor[2].ZeroPos - 550;
            motor[3].DesPos = motor[3].ZeroPos + 550;
            motor[4].DesPos = motor[4].ZeroPos;
        }
    }else{
        set_motor_angle(angle1, angle2, angle3, angle4, angle5);
    }
        
} 

uint8_t rxFlag = 0;
uint8_t rxLen = 0;
uint8_t rxDataLen = 0;
uint8_t id = 0;
uint8_t rxBuffer[30] = {0};
void xgo_rx(){
    uint8_t tempBuf[10];
    uint8_t res = 0; 
    uint8_t checkSum = 0;
    uint16_t POS_LOW_Byte = 0;
    uint16_t POS_HIGH_Byte = 0;
    uint16_t VEL_LOW_Byte = 0;
    uint16_t VEL_HIGH_Byte = 0;
    uint16_t TOR_LOW_Byte = 0;
    uint16_t TOR_HIGH_Byte = 0;
    while(uart_read_bytes(UART_NUM_2, tempBuf, 1, 5) > 0){
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
                    id = res;
                    rxFlag = 3;
                    break;
            case 3:
                if(res == 0x08||res == 0x0B)
                    {					
                        rxFlag = 4; 
                        rxBuffer[3] = res;
                        rxLen = 0;
                        rxDataLen = res;
                        checkSum = 0;
                    }
                else{
                    rxFlag = 0;
                }                    
                break;
            case 4:
                rxBuffer[4+rxLen] = res;
                rxLen++;
                if(rxLen==rxDataLen){
                    for(int i=0; i<1+rxDataLen; i++){
                        checkSum += rxBuffer[2+i];
                    }
                    checkSum = ~checkSum;
                    if(checkSum == rxBuffer[3+rxDataLen]){          
                        POS_LOW_Byte =  rxBuffer[rxDataLen - 1];
                        POS_HIGH_Byte =  rxBuffer[rxDataLen];
                        VEL_LOW_Byte =  rxBuffer[rxDataLen - 3];
                        VEL_HIGH_Byte =  rxBuffer[rxDataLen - 2];
                        TOR_LOW_Byte =  rxBuffer[rxDataLen + 1];
                        TOR_HIGH_Byte =  rxBuffer[rxDataLen + 2];
                        if(id>0&&id<=MOTOR_NUM){
                            id = id - 1;
                            motor[id].FbPos = POS_LOW_Byte | (POS_HIGH_Byte << 8);
                            motor[id].FbSpd = VEL_LOW_Byte | (VEL_HIGH_Byte << 8);
                            motor[id].FbTor = TOR_LOW_Byte | (TOR_HIGH_Byte << 8);
                            
                            id = id + 1;
                            // 检测堵转（位置偏差 + 扭矩）
                            CheckMotorStall(id);
                        }	 
                    }
                    checkSum = 0;
                    rxFlag = 0;                  			
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
                    if (calibrate_mode == 0) {
                        // 进入标定模式：舵机归中并禁用
                        ESP_LOGI("XGO", "Triple click: Enter calibration mode");
                        calibrate_mode = 1;
                        // 显示标定模式表情
                        if (display) {
                            display->SetEmotion("calibration");
                        }
                        short mid_pos[] = {1500, 1500, 1500, 1500, 1500};
                        for (int i = 0; i < 10; i++) {
                            SetMotorAngle(mid_pos, 0);
                            vTaskDelay(pdMS_TO_TICKS(200));
                        }
                        EnableAllMotor(0);
                    } else {
                        // 退出标定模式：保存零点并启用舵机
                        ESP_LOGI("XGO", "Triple click: Exit calibration mode");
                        WriteZeroPos();
                        calibrate_mode = 0;
                        EnableAllMotor(1);
                        // 恢复正常表情
                        if (display) {
                            display->SetEmotion("neutral");
                        }
                    }
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

//Custom Servo Control Function - You can add your own servo commands here
void xgo_control() { 
    if(init_flag == 0){
        return;
    }
    static uint32_t counter = 0;
    static uint32_t counter2 = 0;
    static uint8_t read_id = 1;
    short angle[5] = {0};
    counter++; 
    counter2++;
    
    if(Action_ID==0){
        move();
    }else{        
        if(counter2%5 == 1||counter2%5 == 3){
            xgo_action();
        }
    }
    for(int i=0;i<5;i++){
        angle[i] = motor[i].DesPos;
    }
    if(calibrate_mode == 0){
        SetMotorAngle(angle, motor_speed);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
    if(counter2%20 == 0){
        detect_triple_click();
    }

    if(counter%50 == 0){
        ReadMotorState(read_id);
        counter = 0;
        read_id++;
        if(read_id > 6){
            read_id = 1;
        }
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
        control_mode = 0; // 使用步态控制
        break;
    }
    case 0x32: { // speedVyaw: 原地转向速度
        int w = from_order_range(value, -100, 100);
        vyaw = (float)w;
        control_mode = 0;
        break;
    }
    case 0x3E: { // action: 动作编号
        uint8_t act = value;
        if (act == 0x00) {
            // 停止所有动作，回到默认姿态
            Clear_State(2);
        } else if (act == 0x0F) {
            // Reset 动作，同样视为完全复位
            Clear_State(2);
        } else {
            Action_ID = act;
            // 停止行走速度，让动作更清晰
            vx = 0.0f;
            vyaw = 0.0f;
            control_mode = 0;
        }
        break;
    }
    default:
        // 其他地址暂不处理，保留给后续扩展
        break;
    }
}

