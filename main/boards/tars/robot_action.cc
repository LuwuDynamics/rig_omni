#include "robot_action.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char* TAG = "ROBOT_ACTION";

uint8_t ACTION_DONE = 0;

static int64_t gait_t0_us = 0;
static int64_t gait_until_us = 0;

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int clamp_speed(int speed) {
    if (speed > (int)TARS_GAIT_AMP_MAX) return (int)TARS_GAIT_AMP_MAX;
    if (speed < -(int)TARS_GAIT_AMP_MAX) return -(int)TARS_GAIT_AMP_MAX;
    return speed;
}

void Reset() {
    vx = 0.0f;
    vyaw = 0.0f;
    Action_ID = 0;
    actionLoop_FLAG = 0;
    ACTION_DONE = 0;
    gait_t0_us = 0;
    gait_until_us = 0;
    float zero[MOTOR_NUM] = {0, 0, 0};
    WritePos(zero);
}

void tars_set_vx(int speed) {
    vx = (float)clamp_speed(speed);
    Action_ID = 0;
    control_mode = 0;
    motor_speed = 0;
    gait_until_us = 0;
}

void tars_set_vyaw(int speed) {
    vyaw = (float)clamp_speed(speed);
    Action_ID = 0;
    control_mode = 0;
    motor_speed = 0;
    gait_until_us = 0;
}

void tars_set_forward(int speed, int time_ms) {
    tars_set_vx(speed);
    vyaw = 0.0f;
    gait_t0_us = 0;
    gait_until_us = (time_ms > 0) ? (esp_timer_get_time() + (int64_t)time_ms * 1000) : 0;
    ESP_LOGI(TAG, "forward speed=%d time=%d", speed, time_ms);
}

void tars_set_turn(int speed, int time_ms) {
    tars_set_vyaw(speed);
    vx = 0.0f;
    gait_t0_us = 0;
    gait_until_us = (time_ms > 0) ? (esp_timer_get_time() + (int64_t)time_ms * 1000) : 0;
    ESP_LOGI(TAG, "turn speed=%d time=%d", speed, time_ms);
}

void tars_gait_update() {
    if (gait_until_us > 0 && esp_timer_get_time() >= gait_until_us) {
        vx = 0.0f;
        vyaw = 0.0f;
        gait_until_us = 0;
        gait_t0_us = 0;
    }

    float amp_x = clampf(vx, -TARS_GAIT_AMP_MAX, TARS_GAIT_AMP_MAX);
    float amp_yaw = clampf(vyaw, -TARS_GAIT_AMP_MAX, TARS_GAIT_AMP_MAX);

    const float kDead = 0.5f;
    if (fabsf(amp_x) < kDead && fabsf(amp_yaw) < kDead) {
        gait_t0_us = 0;
        float zero[MOTOR_NUM] = {0, 0, 0};
        WritePos(zero);
        return;
    }

    int64_t now = esp_timer_get_time();
    if (gait_t0_us == 0) {
        gait_t0_us = now;
    }
    float t = (now - gait_t0_us) / 1000000.0f;
    float phase = TARS_GAIT_FREQ_HZ * t * 2.0f * (float)PI;
    float c = cosf(phase);
    float s = sinf(phase);

    float pos[MOTOR_NUM] = {0, 0, 0};

    if (fabsf(amp_yaw) >= fabsf(amp_x)) {
        // 左转 amp>0，右转 amp<0
        // [cos*40, sin*amp, -sin*amp]，参考幅度 30 时第一轴为 40
        float scale = fabsf(amp_yaw) / 30.0f;
        pos[0] = c * 40.0f * scale;
        pos[1] = s * amp_yaw;
        pos[2] = -s * amp_yaw;
    } else if (amp_x > 0.0f) {
        // 前进：amp = -speed，参考幅度 40 时第一轴为 55
        float scale = amp_x / 40.0f;
        pos[0] = c * 55.0f * scale;
        pos[1] = s * (-amp_x);
        pos[2] = s * (-amp_x);
    } else {
        // 后退：amp = +|speed|，参考幅度 40 时第一轴为 50
        float speed = -amp_x;
        float scale = speed / 40.0f;
        pos[0] = c * 50.0f * scale;
        pos[1] = s * speed;
        pos[2] = s * speed;
    }

    WritePos(pos);
}

void robot_action() {
    if (Action_ID == reset_ID) {
        Reset();
    } else {
        Action_ID = 0;
    }
}
