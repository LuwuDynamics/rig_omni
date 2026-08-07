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
#include <esp_timer.h>
#include <esp_random.h>
#include "xgo_action.h"
#include "idle_motion.h"
#include "application.h"
#include "board.h"
#include "display.h"

static const char* TAG = "XGO";

Motor motor[MOTOR_NUM];
uint16_t zero_buffer[MOTOR_NUM] = {410, 655, 485, 525, 395};
uint16_t zero_buffer_default[MOTOR_NUM] = {410, 655, 485, 525, 395};
uint16_t motor_speed = 350;
uint8_t Action_ID = 0;
uint8_t actionLoop_FLAG = 0;
uint8_t serial_lock = 0;

int calibrate_mode = 0;
int init_flag = 0;
float vx = 0.0;
float vyaw = 0.0;

int control_mode = 0; //0为移动模式，1为直接控制角度模式
uint8_t isIMUInit = 0;
float q_head = 0.0;

float angle1 = 0.0;
float angle2 = 0.0;
float angle3 = 0.0;
float angle4 = 0.0;
float angle5 = 0.0;

float arm_angle[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
float arm_x = 0.035;
float arm_y = -0.01;
float arm_z = 0.2;
float arm_yaw = 0.0;
float arm_pitch = -0.3;
float arm_roll = 0.0;
float arm_w = 0.65;
float ctrl_mode = 0;
RigArmIK arm_ik;
// 堵转检测相关变量
static motor_stall_callback_t stall_callback = nullptr;
static bool stall_detection_enabled = false;
static uint32_t last_stall_time = 0;  // 上次触发堵转的时间
static uint8_t stall_count[MOTOR_NUM] = {0};  // 各舵机防抖计数

// ============================================================
// 示教模式全局变量
// ============================================================
TeachState teach_state = TEACH_IDLE;
TeachFrame* teach_frames = nullptr;
uint32_t teach_frame_count = 0;
uint16_t teach_sample_counter = 0;
uint8_t teach_read_servo_id = 1;
uint16_t teach_enter_counter = 0;
bool teach_has_recording = false;

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
        if(data[i]<100||data[i]>900){
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
        printf("Device calibrated, zero positions loaded from flash\n");
    }else{
        // 未标定，进入标定模式：舵机归中并禁用
        printf("Device not calibrated, entering calibration mode\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        calibrate_mode = 1;
        short mid_pos[] = {M_N/2, M_N/2, M_N/2, M_N/2, M_N/2};
        for(int i=0; i<10; i++){
            SetMotorPos(mid_pos, 0);
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

void SetMotorPos(short pos[], short vel) {
    const int l_a = MOTOR_NUM;
    const uint8_t inst_length = (uint8_t)((6 + 1) * l_a + 4);
    uint8_t data_buf[5 + 7 * MOTOR_NUM];
    int idx = 0;

    data_buf[idx++] = 0xFE;
    data_buf[idx++] = inst_length;
    data_buf[idx++] = 0x83;
    data_buf[idx++] = 0x2A;  // ADDR_POS
    data_buf[idx++] = 0x06;

    for (int i = 0; i < l_a; i++) {
        // 舵机有效行程限位：发送前钳制到 [1, 1023]
        short p = pos[i];
        if (p < 1) p = 1;
        if (p > 1023) p = 1023;
        data_buf[idx++] = (uint8_t)(i + 1);
        data_buf[idx++] = (p >> 8) & 0xff;
        data_buf[idx++] = p & 0xff;
        data_buf[idx++] = 0;  // torque low
        data_buf[idx++] = 0;  // torque high
        data_buf[idx++] = (vel >> 8) & 0xff;
        data_buf[idx++] = vel & 0xff;
    }

    uint8_t packet[2 + sizeof(data_buf) + 1];
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    uint8_t check_sum = 0;
    for (int i = 0; i < idx; i++) {
        check_sum += data_buf[i];
        packet[2 + i] = data_buf[i];
    }
    packet[2 + idx] = (uint8_t)(255 - (check_sum & 0xFF));
    SendMotorCommand(packet, 3 + idx);
}

void SetMotorAngle(float angle[],short vel){
    short pos[5];
    for(int i=0;i<5;i++){
        pos[i] = (short)(motor[i].ZeroPos + angle[i]/M_A*M_N);
    }
    // printf("pos: %d, %d, %d, %d, %d\r\n", pos[0], pos[1], pos[2], pos[3], pos[4]);
    SetMotorPos(pos, vel);
}

void ReadMotorState(uint8_t ID){
	uint8_t bBuf[8];
	uint8_t CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[3] = 0x04;
	bBuf[4] = 0x02;
	bBuf[5] = 0x38;
	bBuf[6] = 0x06;
	CheckSum = ID + 0x04 + 0x02 + 0x38 + 0x06;
	bBuf[7] = ~CheckSum;
	SendMotorCommand(bBuf, 8);
}

float servo_voltage = 0.0;  // ID=1 舵机电池电压

void ReadServoVoltage(uint8_t readID){
    uint8_t bBuf[8];
    uint8_t CheckSum = 0;
    bBuf[0] = 0xff;
    bBuf[1] = 0xff;
    bBuf[2] = readID;
    bBuf[3] = 0x04;
    bBuf[4] = 0x02;
    bBuf[5] = 0x3E;      // PRESENT_VOLTAGE 寄存器
    bBuf[6] = 0x01;      // 读取 1 字节
    CheckSum = readID + 0x04 + 0x02 + 0x3E + 0x01;
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
	bBuf[5] = 0x28;
	bBuf[6] = mode;
	CheckSum = ID + 0x04 + 0x03 + 0x28 + mode;
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



uint8_t rxFlag = 0;
uint8_t rxLen = 0;
uint8_t rxDataLen = 0;
uint8_t id = 0;
uint8_t rxBuffer[30] = {0};
void xgo_rx(){
    uint8_t tempBuf[1];
    uint8_t res = 0; 
    uint8_t checkSum = 0;
    uint16_t POS_LOW_Byte  = 0;
    uint16_t POS_HIGH_Byte = 0;
    uint16_t VEL_LOW_Byte  = 0;
    uint16_t VEL_HIGH_Byte = 0;
    uint16_t TOR_LOW_Byte  = 0;
    uint16_t TOR_HIGH_Byte = 0;
    int n;
    while((n = uart_read_bytes(UART_NUM_2, tempBuf, 1, 5)) > 0){
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
                if(res == 0x08||res == 0x0B||res == 0x03)
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
                        if (rxDataLen == 3) {
                            // PRESENT_VOLTAGE 响应: 1 字节电压值 (0.1V 精度)
                            // for(int i=0;i<rxDataLen+3;i++){
                            //     printf("%02X ", rxBuffer[i]);
                            // }
                            // printf("\r\n");
                            servo_voltage = rxBuffer[5] * 0.1f;
                        } else {          
                        POS_LOW_Byte =  rxBuffer[rxDataLen - 3];
                        POS_HIGH_Byte =  rxBuffer[rxDataLen - 2];
                        VEL_LOW_Byte =  rxBuffer[rxDataLen - 1];
                        VEL_HIGH_Byte =  rxBuffer[rxDataLen];
                        TOR_LOW_Byte =  rxBuffer[rxDataLen + 1];
                        TOR_HIGH_Byte =  rxBuffer[rxDataLen + 2];
                        if(id>0&&id<=MOTOR_NUM){
                            id = id - 1;
                            motor[id].FbPos = POS_HIGH_Byte | (POS_LOW_Byte << 8);
                            motor[id].FbSpd = VEL_HIGH_Byte | (VEL_LOW_Byte << 8);
                            motor[id].FbTor = TOR_HIGH_Byte | (TOR_LOW_Byte << 8);
                            // printf("motor[%d].FbPos: %d \r\n", id, motor[id].FbPos);
                            id = id + 1;
                            
                            
                            // 检测堵转（示教模式中跳过：扭矩已关，反馈滞后是正常的）
                            if (teach_state != TEACH_RECORDING) {
                                CheckMotorStall(id);
                            }
                        }
                        
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
    }       // end while
}           // end xgo_rx

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
                        short mid_pos[] = {M_N/2, M_N/2, M_N/2, M_N/2, M_N/2};
                        for (int i = 0; i < 10; i++) {
                            SetMotorPos(mid_pos, 500);
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


// 触摸双击：在 IK 的 pitch/roll 上叠加短暂正弦 bias
static float ik_pitch_bias = 0.0f;
static float ik_roll_bias = 0.0f;
static int64_t wiggle_start_us = 0;
static int wiggle_axis = 0;          // 0=无, 1=pitch, 2=roll
static float wiggle_amp_rad = 0.0f;
static float wiggle_freq_hz = 3.0f;  // 触发时随机 2~4 Hz
static int64_t wiggle_cooldown_until_us = 0;
static const int64_t kWiggleDurUs = 1000000;       // 摆动 1s
static const int64_t kWiggleCooldownUs = 1000000;  // 冷却 1s

void touch_wiggle_trigger() {
    int64_t now = esp_timer_get_time();
    if (now < wiggle_cooldown_until_us) {
        ESP_LOGI(TAG, "Touch wiggle: cooldown");
        return;
    }
    if (calibrate_mode == 1 || teach_state != TEACH_IDLE) {
        return;
    }
    if (wiggle_axis != 0) {
        return;
    }

    wiggle_axis = (esp_random() & 1) ? 1 : 2;
    float amp_deg = 5.0f + (float)(esp_random() % 11);  // 5..15°
    wiggle_amp_rad = amp_deg * (float)M_PI / 180.0f;
    // 频率 [2, 4] Hz
    wiggle_freq_hz = 2.0f + (float)(esp_random() % 1001) / 1000.0f * 2.0f;
    wiggle_start_us = now;
    wiggle_cooldown_until_us = now + kWiggleDurUs + kWiggleCooldownUs;
    ik_pitch_bias = 0.0f;
    ik_roll_bias = 0.0f;
    ESP_LOGI(TAG, "Touch wiggle: axis=%s amp=%.0fdeg freq=%.2fHz",
             wiggle_axis == 1 ? "pitch" : "roll", amp_deg, wiggle_freq_hz);
}

static void update_ik_wiggle_bias() {
    if (wiggle_axis == 0) {
        ik_pitch_bias = 0.0f;
        ik_roll_bias = 0.0f;
        return;
    }
    int64_t now = esp_timer_get_time();
    float t = (now - wiggle_start_us) / 1000000.0f;
    if (t >= 1.0f) {
        wiggle_axis = 0;
        ik_pitch_bias = 0.0f;
        ik_roll_bias = 0.0f;
        return;
    }
    float s = wiggle_amp_rad * sinf(2.0f * (float)M_PI * wiggle_freq_hz * t);
    if (wiggle_axis == 1) {
        ik_pitch_bias = s;
        ik_roll_bias = 0.0f;
    } else {
        ik_roll_bias = s;
        ik_pitch_bias = 0.0f;
    }
}

void arm_ik_update(){
    update_ik_wiggle_bias();

    float arm_q[5];
    bool ok = false;
    ok = rig_arm_ik_solve(&arm_ik,
        arm_x, arm_y, arm_z,
        arm_roll + ik_roll_bias, arm_pitch + ik_pitch_bias, arm_yaw,
        arm_w,
        arm_q,
        NULL, NULL);
    // if(ok){
        for(int i=0;i<5;i++){
            arm_angle[i] = arm_q[i];
        }
    // }
}

// ============================================================
// 示教模式函数
// ============================================================

void teach_enter() {
    if (teach_state != TEACH_IDLE) return;
    if (calibrate_mode == 1) return;  // 标定中不进入示教

    ESP_LOGI(TAG, "Teach: entering, allocating PSRAM buffer...");

    // 分配 / 复用 PSRAM 帧缓冲（重新示教会覆盖旧轨迹）
    if (teach_frames == nullptr) {
        teach_frames = (TeachFrame*)heap_caps_malloc(
            TEACH_MAX_FRAMES * sizeof(TeachFrame),
            MALLOC_CAP_SPIRAM);
        if (teach_frames == nullptr) {
            ESP_LOGE(TAG, "Teach: PSRAM allocation failed!");
            return;
        }
    }

    teach_frame_count = 0;
    teach_sample_counter = 0;
    teach_read_servo_id = 1;
    teach_enter_counter = 0;
    teach_has_recording = false;  // 新一轮录制开始，旧轨迹作废直至录制完成

    // ENTERING 阶段：IK 驱动到 stand
    teach_state = TEACH_ENTERING;
    ESP_LOGI(TAG, "Teach: ENTERING — driving to stand pose");
}

void teach_exit() {
    if (teach_state != TEACH_RECORDING) return;

    ESP_LOGI(TAG, "Teach: EXITING — stop recording, %lu frames (%.1fs)",
             teach_frame_count, teach_frame_count * TEACH_SAMPLE_MS / 1000.0f);

    // EXITING：开扭矩，IK 回到初始构型；结束后不自动回放
    EnableAllMotor(1);
    motor_speed = 2000;
    teach_enter_counter = 0;
    teach_state = TEACH_EXITING;
}

void teach_cancel() {
    if (teach_state == TEACH_IDLE && !teach_has_recording && teach_frames == nullptr) return;

    ESP_LOGI(TAG, "Teach: CANCELLED");

    if (teach_state == TEACH_RECORDING) {
        EnableAllMotor(1);
    }

    if (teach_frames != nullptr) {
        heap_caps_free(teach_frames);
        teach_frames = nullptr;
    }

    teach_state = TEACH_IDLE;
    teach_frame_count = 0;
    teach_has_recording = false;
    motor_speed = 800;
    Clear_State(2);
}

int teach_play() {
    if (!teach_has_recording || teach_frames == nullptr || teach_frame_count == 0) {
        ESP_LOGW(TAG, "Teach play: no recording available");
        return -1;
    }
    if (teach_state != TEACH_IDLE) {
        ESP_LOGW(TAG, "Teach play: busy, state=%d", teach_state);
        return -1;
    }
    if (calibrate_mode == 1) {
        return -1;
    }

    ESP_LOGI(TAG, "Teach play: %lu frames", teach_frame_count);
    teach_state = TEACH_REPLAYING;
    Action_ID = TeachPlayback_ID;
    actionLoop_FLAG = 0;
    return 0;
}

//Custom Servo Control Function - You can add your own servo commands here
void xgo_control() {
    if(init_flag == 0){
        return;
    }
    static uint32_t counter = 0;
    static uint32_t counter2 = 0;
    static uint8_t read_id = 1;

    counter++;
    counter2++;

    // ============================================================
    // 示教模式状态机
    // ============================================================
    if (teach_state != TEACH_IDLE) {

        // --- ENTERING: IK 驱动到 stand，1.5s 后关扭矩 ---
        if (teach_state == TEACH_ENTERING) {
            arm_x = 0.035; arm_y = -0.01; arm_z = 0.2;
            arm_yaw = 0.0; arm_pitch = -0.3; arm_roll = 0.0;
            arm_w = 0.65;
            motor_speed = 800;
            arm_ik_update();
            SetMotorAngle(arm_angle, motor_speed);

            if (++teach_enter_counter > 750) {  // 750 × 2ms = 1.5s
                ESP_LOGI(TAG, "Teach: torque off, start recording");
                EnableAllMotor(0);
                teach_sample_counter = 0;
                teach_read_servo_id = 1;
                teach_state = TEACH_RECORDING;
            }
            // 正常轮询反馈
            if (counter % 50 == 0) {
                ReadMotorState(read_id);
                counter = 0;
                if (++read_id > MOTOR_NUM) read_id = 1;
            }
            return;
        }

        // --- RECORDING: 扭矩已关，快速轮询 + 100ms 存帧，最长 15s ---
        if (teach_state == TEACH_RECORDING) {
            ReadMotorState(teach_read_servo_id);
            if (++teach_read_servo_id > MOTOR_NUM) {
                teach_read_servo_id = 1;
            }

            if (++teach_sample_counter >= TEACH_SAMPLE_CYCLES
                && teach_frame_count < TEACH_MAX_FRAMES) {
                teach_sample_counter = 0;
                TeachFrame* f = &teach_frames[teach_frame_count];
                for (int i = 0; i < MOTOR_NUM; i++) {
                    // 存相对 ZeroPos 的偏移，回放时 set_motor_pos 再加回
                    f->pos[i] = motor[i].FbPos - motor[i].ZeroPos;
                }
                teach_frame_count++;
            }

            // 达到最长示教时长，自动结束录制（回到初始构型，不回放）
            if (teach_frame_count >= TEACH_MAX_FRAMES) {
                ESP_LOGI(TAG, "Teach: max duration reached (%ds), auto stop",
                         TEACH_MAX_DURATION_MS / 1000);
                teach_exit();
            }
            return;
        }

        // --- EXITING: 扭矩已开，IK 回到初始构型，结束后空闲（不自动回放）---
        if (teach_state == TEACH_EXITING) {
            arm_x = 0.035; arm_y = -0.01; arm_z = 0.2;
            arm_yaw = 0.0; arm_pitch = -0.3; arm_roll = 0.0;
            arm_w = 0.65;
            motor_speed = 1000;
            arm_ik_update();
            SetMotorAngle(arm_angle, motor_speed);

            if (++teach_enter_counter > 750) {  // 750 × 2ms = 1.5s
                teach_has_recording = (teach_frame_count > 0);
                teach_state = TEACH_IDLE;
                Action_ID = 0;
                motor_speed = 800;
                ESP_LOGI(TAG, "Teach: back to stand, recording ready=%d, frames=%lu",
                         teach_has_recording, teach_frame_count);
            }
            if (counter % 50 == 0) {
                ReadMotorState(read_id);
                counter = 0;
                if (++read_id > MOTOR_NUM) read_id = 1;
            }
            return;
        }

        // --- REPLAYING: 每控制周期插值一步，相邻关键帧固定 40 步 ---
        if (teach_state == TEACH_REPLAYING) {
            xgo_action();  // teach_playback() 线性插值写入 DesPos
            short pos[5];
            for (int i = 0; i < MOTOR_NUM; i++) {
                pos[i] = motor[i].DesPos;
            }
            SetMotorPos(pos, motor_speed);
            if (counter % 50 == 0) {
                ReadMotorState(read_id);
                counter = 0;
                if (++read_id > MOTOR_NUM) read_id = 1;
            }
            return;
        }
    }

    // ============================================================
    // 原有正常控制逻辑（teach_state == TEACH_IDLE）
    // ============================================================

    if(Action_ID==0){
        if(ctrl_mode == 0){
            arm_ik_update();
            idle_motion_update();  // 空闲微动（叠加 q0 ±15°、q4 ±10° 正弦偏移）
        }
    }else{
        if(counter2%5 == 1||counter2%5 == 3){
            xgo_action();
        }
        // 将动作设置的 DesPos 转换为 arm_angle，统一由 SetMotorAngle 发送
        for(int i = 0; i < MOTOR_NUM; i++){
            arm_angle[i] = (motor[i].DesPos - motor[i].ZeroPos) * M_A / M_N;
        }
    }

    if(calibrate_mode == 0){
        SetMotorAngle(arm_angle, motor_speed);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
    if(counter2%20 == 0){
        detect_triple_click();
    }

    if(counter%50 == 0){
        ReadMotorState(read_id);
        counter = 0;
        read_id++;
        if(read_id > MOTOR_NUM){
            read_id = 1;
        }

        // 每分钟读一次电池电压（1200 个周期 × 50ms = 60s）
        static int voltage_counter_puppy = 0;
        if (++voltage_counter_puppy % 1200 == 0) {
            ReadServoVoltage(1);
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
        if (act == 0x00 || act == reset_ID) {
            // 停止所有动作 / Reset：回到开机构型
            Action_ID = reset_ID;
            vx = 0.0f;
            vyaw = 0.0f;
            control_mode = 0;
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

