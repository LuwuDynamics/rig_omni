#include "idle_motion.h"
#include "xgo.h"
#include <math.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_log.h>

static const char* TAG = "IDLE_MOTION";

// 默认打开
static bool enabled = true;

// 正弦相位 (rad) — 达到 2π 时完成一个周期，触发换频
static float phase_q0 = 0.0f;
static float phase_q4 = 0.0f;

// 当前频率 (Hz) — 每个周期结束时重新随机
static float freq_q0 = 0.0f;
static float freq_q4 = 0.0f;

// 上一次更新时间 (us)
static int64_t last_update_us = 0;

// ============================================================
// 参数 (可根据需要调整)
// ============================================================

// q0 (base yaw):  左右摇头 ±15°
#define IDLE_AMP_Q0  15.0f

// q4 (wrist pitch): 上下点头 ±10°
#define IDLE_AMP_Q4  10.0f

// 随机频率范围：周期 = 1s ~ 6s → 频率 = 0.17Hz ~ 1Hz
#define IDLE_PERIOD_MIN  1.5f
#define IDLE_PERIOD_MAX  6.0f

// ============================================================
// 辅助：从周期范围生成随机频率 (Hz)
// ============================================================
static float random_freq() {
    float r = (float)esp_random() / (float)UINT32_MAX;           // [0, 1]
    float period = IDLE_PERIOD_MIN + r * (IDLE_PERIOD_MAX - IDLE_PERIOD_MIN);  // [0.5, 3.0] s
    return 1.0f / period;                                        // [0.33, 2.0] Hz
}

static void maybe_refresh_freq(float* phase, float* freq) {
    if (*phase >= 2.0f * (float)M_PI) {
        *phase -= 2.0f * (float)M_PI;
        *freq = random_freq();
    }
}

void idle_motion_init() {
    enabled = true;
    phase_q0 = 0.0f;
    phase_q4 = 0.0f;
    freq_q0 = random_freq();
    freq_q4 = random_freq();
    last_update_us = 0;
    ESP_LOGI(TAG, "Idle motion initialized (q0_freq=%.2fHz, q4_freq=%.2fHz)", freq_q0, freq_q4);
}

void idle_motion_update() {
    if (!enabled) return;

    int64_t now = esp_timer_get_time();
    if (last_update_us == 0) {
        last_update_us = now;
        return;
    }

    float dt = (now - last_update_us) / 1000000.0f;
    last_update_us = now;

    // 安全防护：dt 异常大（如启动后首帧）则跳过本帧
    if (dt <= 0.0f || dt > 0.1f) return;

    // 推进相位（用当前频率）
    phase_q0 += 2.0f * (float)M_PI * freq_q0 * dt;
    phase_q4 += 2.0f * (float)M_PI * freq_q4 * dt;

    // 周期完成时换一个新的随机频率
    maybe_refresh_freq(&phase_q0, &freq_q0);
    maybe_refresh_freq(&phase_q4, &freq_q4);

    // 叠加到关节角：arm_angle[] 单位是度
    arm_angle[0] += IDLE_AMP_Q0 * sinf(phase_q0);   // q0: 左右微摇
    arm_angle[4] += IDLE_AMP_Q4 * cosf(phase_q4);   // q4: 上下微点
}

void idle_motion_set_enable(bool enable) {
    enabled = enable;
    if (!enable) {
        // 关闭时重置相位，下次打开从头开始
        phase_q0 = 0.0f;
        phase_q4 = 0.0f;
    }
    last_update_us = 0;
    ESP_LOGI(TAG, "Idle motion %s", enabled ? "ON" : "OFF");
}

bool idle_motion_is_enabled() {
    return enabled;
}
