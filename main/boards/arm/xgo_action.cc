#include "xgo_action.h"
#include "math.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

static const char* TAG = "XGO_ACTION";

uint16_t Action_Counter[ACTION_NUMBER];

uint8_t ACTION_DONE = 0;

void action_loop(){
    if(ACTION_DONE&&actionLoop_FLAG){
        ACTION_DONE++;
        if(ACTION_DONE==100)
        { 
            ACTION_DONE=0;
            Action_ID++;
            if(Action_ID==ACTION_NUMBER){
                Action_ID = 1;
            }
        }
    }
}

void xgo_action(){ 
    static uint8_t last_action_id = 0;
    if (Action_ID != last_action_id) {
        if (Action_ID < ACTION_NUMBER) {
            ESP_LOGI(TAG, "Action started: %d, Counter: %d", Action_ID, Action_Counter[Action_ID]);
        } else {
            ESP_LOGI(TAG, "Action started: %d", Action_ID);
        }
        last_action_id = Action_ID;
    }
    action_loop(); 
    switch(Action_ID)
    {
        case TeachPlayback_ID:
            teach_playback();
            break;
        case Stretch_ID:
            Stretch();
            break;
        case Apology_ID:
            Apology();
            break;
        case Shy_ID:
            Shy();
            break;
        case Coquetry_ID:
            Coquetry();
            break;
        case Bow_ID:
            Bow();
            break;
        case FeignDeath_ID:
            FeignDeath();
            break;
        case WagTail_ID:
            WagTail();
            break;
        case reset_ID:
            Reset();
            break;
        case 0:
            break;
        default:
            Action_ID = 0;
            break;
    }    
    Updated_Counter();
}

static short clamp_servo_pos(int pos) {
    if (pos < 1) return 1;
    if (pos > 1023) return 1023;
    return (short)pos;
}

void set_motor_pos(int p1, int p2, int p3, int p4, int p5){
    motor[0].DesPos = clamp_servo_pos(motor[0].ZeroPos + p1);
    motor[1].DesPos = clamp_servo_pos(motor[1].ZeroPos + p2);
    motor[2].DesPos = clamp_servo_pos(motor[2].ZeroPos + p3);
    motor[3].DesPos = clamp_servo_pos(motor[3].ZeroPos + p4);
    motor[4].DesPos = clamp_servo_pos(motor[4].ZeroPos + p5);
}

void normal_state(){  
    motor_speed = 500; 
    arm_x = 0.035f;
    arm_y = -0.01f;
    arm_z = 0.2f;
    arm_yaw = 0.0f;
    arm_pitch = -0.3f;
    arm_roll = 0.0f;
    arm_w = 0.65f;
    control_mode = 0;    
}

void Updated_Counter(){	
	for(int i=0;i<ACTION_NUMBER;i++){
        if(i==Action_ID)
            Action_Counter[i]++;
        else
            Action_Counter[i] = 0;
    }
}

void Clear_State(uint8_t mode){
    normal_state();
    if (mode==0){
        ACTION_DONE = 0;
    }   
    if (mode==1){
        ACTION_DONE = 1;
        if(actionLoop_FLAG==0){            
            Action_ID = 0;
        }
    }

    if (mode == 2){
        actionLoop_FLAG = 0;
        ACTION_DONE = 0;
        Action_ID = 0;
    }  
}

// 复位到开机构型（与 xgo.cc 中 arm_* 初始值一致）
void Reset(){
    Clear_State(2);
}

// 伸懒腰 — 2026-07-22 示教录制 → 固化 v3（偏移量，不取反）
// 抬身 → 摆腕 → 深前伸 → 长保持。约 5.2 秒
void Stretch(){
    //   阶段:  [0]起始  [1]抬起  [2]摆腕  [3]深伸  [4]保持
    float duration[] = {0.3, 0.6, 1.8, 1.0, 1.5};
    uint16_t timepoint[6] = {0};
    for(int i=1;i<6;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Stretch_ID];
    float phase = counter*2.0f*(float)M_PI/TS;

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 3000;
        set_motor_pos(-5, -152, -341, 0, -250);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        set_motor_pos(-5, -152, -341, 0, -250);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        float t = (counter - timepoint[1]) / (float)(timepoint[2] - timepoint[1]);
        int s1 = -152 + (int)(( 34 + 152) * t);
        int s2 = -341 + (int)(( 41 + 341) * t);
        set_motor_pos(-5, s1, s2, 0, -250);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        int s1 = 34;
        int s2 = 41;
        int s3 = (int)(30 * sinf(phase * 1.8f));
        set_motor_pos(-5, s1, s2, s3, -250);
    }else if(counter > timepoint[3] && counter <= timepoint[4]){
        float t = (counter - timepoint[3]) / (float)(timepoint[4] - timepoint[3]);
        int s1 =   34 + (int)((-184 -  34) * t);
        int s2 =   41 + (int)((-386 -  41) * t);
        int s3 = (int)(30 * (1.0f - t) * sinf(phase * 2.5f));
        motor_speed = 2500;
        set_motor_pos(-5, s1, s2, s3, -252);
    }else if(counter > timepoint[4] && counter < timepoint[5]){
        set_motor_pos(-5, -184, -386, 0, -252);
    }else if(counter >= timepoint[5]){
        Clear_State(1);
    }
}

// 谢罪道歉 — 2026-07-22 示教录制 → 固化 v2（偏移量，不取反）
void Apology(){
    float duration[] = {0.5, 1.0, 2.5, 1.2, 1.8};
    uint16_t timepoint[6] = {0};
    for(int i=1;i<6;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Apology_ID];
    float phase = counter*2.0f*(float)M_PI/TS;

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 2000;
        set_motor_pos(-2, -120, -305, 2, -247);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        set_motor_pos(-2, -120, -305, 2, -247);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        float t = (counter - timepoint[1]) / (float)(timepoint[2] - timepoint[1]);
        int s1 = -120 + (int)((157 + 120) * t);
        int s2 = -305 + (int)((-224 + 305) * t);
        int s4 = -247 + (int)((-54  + 247) * t);
        set_motor_pos(-1, s1, s2, 2, s4);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        int s1 =  100 + (int)(50 * sinf(phase * 1.0f));
        int s2 = -280 + (int)(30 * sinf(phase * 1.3f));
        int s4 =  -80 + (int)(15 * cosf(phase * 0.7f));
        set_motor_pos(0, s1, s2, 2, s4);
    }else if(counter > timepoint[3] && counter <= timepoint[4]){
        float t = (counter - timepoint[3]) / (float)(timepoint[4] - timepoint[3]);
        int s1 =  120 + (int)((-330 - 120) * t);
        int s2 = -280 + (int)((-503 + 280) * t);
        int s4 =  -85 + (int)((-187 +  85) * t);
        motor_speed = 1500;
        set_motor_pos(-5, s1, s2, 1, s4);
    }else if(counter > timepoint[4] && counter < timepoint[5]){
        set_motor_pos(-11, -330, -503, 1, -187);
    }else if(counter >= timepoint[5]){
        Clear_State(1);
    }
}

void node(){
    float pitch_init = arm_pitch;
    motor_speed = 1000;
    long t0 = esp_timer_get_time() / 1000000;
    float phase = 0.0;
    float freq = 3.0;
    while(esp_timer_get_time() / 1000000 - t0 < 2){
        phase = 2.0*PI*freq*(esp_timer_get_time() / 1000000.0 - t0);
        arm_pitch = pitch_init + 0.3*sin(phase);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    motor_speed = 350;
    arm_pitch = pitch_init;
}

void shake(){
    float yaw_init = arm_yaw;
    motor_speed = 1000;
    long t0 = esp_timer_get_time() / 1000000;
    float phase = 0.0;
    float freq = 2.0;
    while(esp_timer_get_time() / 1000000 - t0 < 2){
        phase = 2.0*PI*freq*(esp_timer_get_time() / 1000000.0 - t0);
        arm_yaw = yaw_init + 0.3*sin(phase);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    motor_speed = 350;
    arm_yaw = yaw_init;
}

// ============================================================
// 示教回放 — 相邻关键帧固定 40 步线性插值，不释放轨迹
// ============================================================
void teach_playback() {
    if (teach_frames == nullptr || teach_frame_count == 0) {
        ESP_LOGW(TAG, "Teach playback: no frames to play");
        teach_state = TEACH_IDLE;
        Clear_State(1);
        return;
    }

    const uint16_t ticks = TEACH_PLAYBACK_TICKS_PER_FRAME;
    uint16_t counter = Action_Counter[TeachPlayback_ID];

    // N 帧 → (N-1) 段插值；单帧则停留 1 个采样周期后结束
    uint32_t span = (teach_frame_count > 1) ? (teach_frame_count - 1) : 1;
    uint32_t max_tick = span * ticks;

    if (counter >= max_tick) {
        TeachFrame* last = &teach_frames[teach_frame_count - 1];
        set_motor_pos(last->pos[0], last->pos[1], last->pos[2], last->pos[3], last->pos[4]);
        ESP_LOGI(TAG, "Teach playback: done (%lu frames @ %dms, interpolated), trajectory kept",
                 teach_frame_count, TEACH_SAMPLE_MS);
        teach_state = TEACH_IDLE;
        motor_speed = 800;
        Clear_State(1);
        return;
    }

    uint32_t i0 = counter / ticks;
    if (i0 >= teach_frame_count - 1) {
        i0 = teach_frame_count - 1;
    }
    uint32_t i1 = (i0 + 1 < teach_frame_count) ? (i0 + 1) : i0;
    float t = (float)(counter % ticks) / (float)ticks;

    int p[MOTOR_NUM];
    for (int j = 0; j < MOTOR_NUM; j++) {
        float a = (float)teach_frames[i0].pos[j];
        float b = (float)teach_frames[i1].pos[j];
        p[j] = (int)(a + (b - a) * t);
    }
    set_motor_pos(p[0], p[1], p[2], p[3], p[4]);
    motor_speed = 0;  // 最快跟随插值目标
}

// 害羞 — 2026-07-22 示教录制 → 固化（偏移量，不取反）
void Shy(){
    float duration[] = {0.5, 1.2, 0.5, 2.5, 2.0, 2.0};
    uint16_t timepoint[7] = {0};
    for(int i=1;i<7;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Shy_ID];
    float phase = counter*2.0f*(float)M_PI/TS;

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 2500;
        set_motor_pos(-4, -161, -343, 0, -247);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        set_motor_pos(-4, -161, -343, 0, -247);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        float t = (counter - timepoint[1]) / (float)(timepoint[2] - timepoint[1]);
        int s1 = -161 + (int)((-280 + 161) * t);
        int s2 = -343 + (int)((-400 + 343) * t);
        int s4 = -247 + (int)((  -6 + 247) * t);
        set_motor_pos(-4, s1, s2, 0, s4);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        float t = (counter - timepoint[2]) / (float)(timepoint[3] - timepoint[2]);
        int s1 = -280 + (int)((-267 + 280) * t);
        int s2 = -400 + (int)((-370 + 400) * t);
        int s4 =   -6 + (int)(( 141 +   6) * t);
        motor_speed = 2000;
        set_motor_pos(0, s1, s2, 0, s4);
    }else if(counter > timepoint[3] && counter <= timepoint[4]){
        int s0 = (int)(30 * sinf(phase * 1.2f));
        int s1 = -270 + (int)(20 * sinf(phase * 1.5f));
        int s2 = -380 + (int)(10 * cosf(phase * 1.0f));
        set_motor_pos(s0, s1, s2, 0, 140);
    }else if(counter > timepoint[4] && counter <= timepoint[5]){
        float t = (counter - timepoint[4]) / (float)(timepoint[5] - timepoint[4]);
        int s0 = (int)( 21 * t + 20 * (1.0f - t) * sinf(phase * 0.8f));
        int s1 = -270 + (int)((-315 + 270) * t);
        int s2 = -380 + (int)((-506 + 380) * t);
        int s4 =  140 + (int)((-303 - 140) * t);
        motor_speed = 2000;
        set_motor_pos(s0, s1, s2, 0, s4);
    }else if(counter > timepoint[5] && counter < timepoint[6]){
        set_motor_pos(21, -315, -506, 0, -303);
    }else if(counter >= timepoint[6]){
        Clear_State(1);
    }
}

// 撒娇 — 2026-07-22 示教录制 → 固化（偏移量，不取反）
void Coquetry(){
    float duration[] = {0.5, 3.5, 1.0, 0.7};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Coquetry_ID];
    float phase = counter*2.0f*(float)M_PI/TS;

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 2000;
        set_motor_pos(-3, -238, -476, -1, -249);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        float t = (counter - timepoint[0]) / (float)(timepoint[1] - timepoint[0]);
        int s0 =  -3 + (int)((-50 +   3) * t);
        int s1 = -238 + (int)((-341 + 238) * t);
        int s2 = -476 + (int)((-500 + 476) * t);
        set_motor_pos(s0, s1, s2, -1, -247);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        int s3 = (int)(120 * sinf(phase * 0.45f));
        int s0 = -50 + (int)(15 * cosf(phase * 0.45f));
        set_motor_pos(s0, -341, -500, s3, -247);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        float t = (counter - timepoint[2]) / (float)(timepoint[3] - timepoint[2]);
        float amp = 120 * (1.0f - t);
        int s3 = (int)(amp * sinf(phase * 0.38f));
        int s0 = -50 + (int)(10 * (1.0f - t) * cosf(phase * 0.38f));
        set_motor_pos(s0, -341, -500, s3, -247);
    }else if(counter > timepoint[3] && counter < timepoint[4]){
        set_motor_pos(-50, -341, -500, -1, -247);
    }else if(counter >= timepoint[4]){
        Clear_State(1);
    }
}

// 鞠躬 — 2026-07-22 示教录制 → 固化（偏移量，不取反）
void Bow(){
    float duration[] = {0.5, 1.0, 0.8, 1.2, 1.0, 0.5};
    uint16_t timepoint[7] = {0};
    for(int i=1;i<7;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[Bow_ID];

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 1300;
        set_motor_pos(3, -107, -207, 4, -137);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        set_motor_pos(3, -107, -207, 4, -137);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        float t = (counter - timepoint[1]) / (float)(timepoint[2] - timepoint[1]);
        int s1 = -107 + (int)((-32  + 107) * t);
        int s2 = -207 + (int)((-27  + 207) * t);
        int s4 = -137 + (int)((-34  + 137) * t);
        set_motor_pos(3, s1, s2, 4, s4);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        set_motor_pos(3, -32, -27, 4, -34);
    }else if(counter > timepoint[3] && counter <= timepoint[4]){
        float t = (counter - timepoint[3]) / (float)(timepoint[4] - timepoint[3]);
        int s2 = -27 + (int)((-331 + 27) * t);
        motor_speed = 2000;
        set_motor_pos(3, -32, s2, 4, -34);
    }else if(counter > timepoint[4] && counter <= timepoint[5]){
        float t = (counter - timepoint[4]) / (float)(timepoint[5] - timepoint[4]);
        int s2 = -331 + (int)((-27 + 331) * t);
        set_motor_pos(3, -32, s2, 4, -34);
    }else if(counter > timepoint[5] && counter < timepoint[6]){
        set_motor_pos(3, -32, -27, 4, -34);
    }else if(counter >= timepoint[6]){
        Clear_State(1);
    }
}

// 装死 — 2026-07-22 示教录制 → 固化（偏移量，不取反）
void FeignDeath(){
    float duration[] = {0.3, 0.8, 2.0, 2.0};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[FeignDeath_ID];

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 3000;
        set_motor_pos(-4, -160, -353, -1, -251);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        float t = (counter - timepoint[0]) / (float)(timepoint[1] - timepoint[0]);
        int s1 = -160 + (int)((-178 + 160) * t);
        int s2 = -353 + (int)((-289 + 353) * t);
        int s4 = -251 + (int)((-157 + 251) * t);
        set_motor_pos(-4, s1, s2, 2, s4);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        set_motor_pos(-4, -178, -289, 2, -157);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        float t = (counter - timepoint[2]) / (float)(timepoint[3] - timepoint[2]);
        int s0 =  -4 + (int)((-18 +   4) * t);
        int s1 = -178 + (int)((-324 + 178) * t);
        int s2 = -289 + (int)((-96  + 289) * t);
        int s4 = -157 + (int)((-159 + 157) * t);
        motor_speed = 1500;
        set_motor_pos(s0, s1, s2, 2, s4);
    }else if(counter > timepoint[3] && counter < timepoint[4]){
        motor_speed = 600;
        set_motor_pos(-18, -324, -96, 2, -159);
    }else if(counter >= timepoint[4]){
        Clear_State(1);
    }
}

// 摇尾巴 — 2026-07-22 示教录制 → 固化（偏移量，不取反）
void WagTail(){
    float duration[] = {0.5, 3.0, 1.5, 0.8};
    uint16_t timepoint[5] = {0};
    for(int i=1;i<5;i++){
        timepoint[i] = timepoint[i-1] + duration[i-1]*TS;
    }
    uint16_t counter = Action_Counter[WagTail_ID];
    float phase = counter*2.0f*(float)M_PI/TS;

    if(counter == timepoint[0]){
        Clear_State(0);
        motor_speed = 1500;
        set_motor_pos(-4, -147, -465, -2, -313);
    }else if(counter > timepoint[0] && counter <= timepoint[1]){
        float t = (counter - timepoint[0]) / (float)(timepoint[1] - timepoint[0]);
        int s1 = -147 + (int)((-101 + 147) * t);
        int s2 = -465 + (int)((-505 + 465) * t);
        set_motor_pos(-4, s1, s2, -3, -313);
    }else if(counter > timepoint[1] && counter <= timepoint[2]){
        float t = (counter - timepoint[1]) / (float)(timepoint[2] - timepoint[1]);
        float amp = 90.0f * (1.0f - 0.5f * t);
        int s0 = (int)(amp * sinf(phase * 2.0f));
        set_motor_pos(s0, -101, -505, -3, -313);
    }else if(counter > timepoint[2] && counter <= timepoint[3]){
        float t = (counter - timepoint[2]) / (float)(timepoint[3] - timepoint[2]);
        float amp = 45.0f * (1.0f - t);
        int s0 = (int)(amp * sinf(phase * 1.5f));
        set_motor_pos(s0, -101, -505, -3, -313);
    }else if(counter > timepoint[3] && counter < timepoint[4]){
        set_motor_pos(-4, -101, -505, -3, -313);
    }else if(counter >= timepoint[4]){
        Clear_State(1);
    }
}
