#include "tars_display.h"

#include <cstdio>
#include <cmath>
#include <cstring>

#include <esp_log.h>
#include <lvgl.h>

extern "C" const lv_font_t lv_font_unscii_8;
extern "C" const lv_font_t tars_log_font_6;
LV_FONT_DECLARE(font_maison_neue_book_12);

#include "board.h"
#include "imu.h"
#include "robot.h"

namespace {

constexpr char kTag[] = "TarsDisplay";
// Use a saturated, deep blue for the TARS flight-log channel.  Keeping the
// green channel low avoids the pale cyan cast of the previous palette.
constexpr uint32_t kLogBlue = 0x2867D8;
constexpr uint32_t kLogBlueDim = 0x153D9B;
constexpr uint32_t kSystemGreen = 0x8CDD9F;
constexpr uint32_t kSystemGreenDim = 0x57986B;
constexpr uint32_t kPanelAmber = 0xF2B254;
constexpr uint32_t kPanelRed = 0xF06A54;

constexpr const char* kFlightLog[] = {
    "SYS BOOT", "BUS CHECK", "NAV LOCK", "IMU SAMPLE",
    "M1 FEEDBACK", "M2 FEEDBACK", "M3 FEEDBACK", "CAM READY",
    "AUDIO IDLE", "MESH LINK", "WATCHDOG OK", "CTRL TICK",
    "POSE FILTER", "POWER SAMPLE", "TOUCH READY", "SERVO POLL",
    "MOTION LOOP", "CORE CHECK", "LINK PULSE", "LOG COMMIT",
};

bool IsOneOf(const char* value, const char* const* choices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(value, choices[i]) == 0) {
            return true;
        }
    }
    return false;
}

void ConfigureTextPanel(gfx_obj_t* label, int x, int y, int width, int height,
                        gfx_text_align_t text_align) {
    gfx_obj_align(label, GFX_ALIGN_TOP_LEFT, x, y);
    gfx_obj_set_size(label, width, height);
    gfx_label_set_text_align(label, text_align);
    gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
    gfx_label_set_bg_enable(label, false);
    gfx_obj_set_visible(label, true);
}

constexpr int kCalibErrorIconSize = 48;
// 标定写入失败图标：红色圆环 + 红色感叹号（RGB565，黑底）。像素在首次
// 使用时构建一次，避免在源码里手写上千字节的位图数据。
// 注意：LCD 需要大端序 RGB565（参考 EmoteDisplay::SetPreviewRgb565 的
// bswap16 处理），红色 0xF800 需写成字节交换后的 0x00F8。
uint16_t s_calib_error_pixels[kCalibErrorIconSize * kCalibErrorIconSize];
bool s_calib_error_ready = false;

void BuildCalibErrorIcon() {
    if (s_calib_error_ready) {
        return;
    }
    constexpr int n = kCalibErrorIconSize;
    constexpr float center = (n - 1) * 0.5f;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            const float dx = x - center;
            const float dy = y - center;
            const float radius = std::sqrt(dx * dx + dy * dy);
            const bool ring = radius >= 19.5f && radius <= 23.5f;
            const bool bar = x >= 22 && x <= 25 && y >= 8 && y <= 27;
            const bool dot = x >= 21 && x <= 26 && y >= 31 && y <= 37;
            s_calib_error_pixels[y * n + x] = (ring || bar || dot) ? 0x00F8 : 0x0000;
        }
    }
    s_calib_error_ready = true;
}

gfx_image_dsc_t kCalibErrorIconDsc = {
    .header = {
        .magic = C_ARRAY_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = kCalibErrorIconSize,
        .h = kCalibErrorIconSize,
        .stride = kCalibErrorIconSize * 2,
        .reserved = 0,
    },
    .data_size = sizeof(s_calib_error_pixels),
    .data = reinterpret_cast<const uint8_t*>(s_calib_error_pixels),
};

}  // namespace

TarsDisplay::TarsDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io,
                         int width, int height)
    : EmoteDisplay(panel, panel_io, width, height), mutex_(xSemaphoreCreateMutex()) {
    width_ = width;
    height_ = height;
}

TarsDisplay::~TarsDisplay() {
    if (refresh_timer_ != nullptr) {
        esp_timer_stop(refresh_timer_);
        esp_timer_delete(refresh_timer_);
    }
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
}

void TarsDisplay::Begin() {
    if (started_ || GetEmoteHandle() == nullptr) {
        return;
    }

    // TARS is a text console rather than an emoji face.  The expression
    // runtime provides the framebuffer and font renderer; the console labels
    // themselves are created in code so they do not rely on named face-layout
    // elements from the generic Emote asset format.
    left_header_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_left_header");
    left_body_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_left_body");
    right_header_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_right_header");
    right_body_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_right_body");
    divider_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_divider");
    // Draw the 12 px footer three times with a one-pixel offset.  The emote
    // renderer has no font-weight setting, so this produces a clearly bold,
    // compact panel typeface without increasing the text size.
    footer_left_stroke_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_left_stroke");
    footer_left_weight_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_left_weight");
    footer_left_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_left");
    footer_right_stroke_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_right_stroke");
    footer_right_weight_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_right_weight");
    footer_right_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_footer_right");
    // The assets partition can lag behind an app-only OTA update.  Do not
    // depend on layout.json having already created eye_anim: it is only used
    // for the Wi-Fi provisioning overlay, while the TARS text console must be
    // available with either the current or a previously installed asset pack.
    wifi_config_animation_ = emote_create_obj_by_type(
        GetEmoteHandle(), EMOTE_OBJ_TYPE_ANIM, "eye_anim");
    calib_error_icon_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_IMAGE, "tars_calib_error");
    calib_error_label_ = emote_create_obj_by_type(GetEmoteHandle(), EMOTE_OBJ_TYPE_LABEL, "tars_calib_error_label");

    if (!left_header_ || !left_body_ || !right_header_ || !right_body_ || !divider_ ||
        !footer_left_ || !footer_left_weight_ || !footer_left_stroke_ ||
        !footer_right_ || !footer_right_weight_ || !footer_right_stroke_ ||
        !wifi_config_animation_ || !calib_error_icon_ || !calib_error_label_) {
        ESP_LOGE(kTag, "Failed to create TARS text console");
        return;
    }

    emote_lock(GetEmoteHandle());
    ConfigureTextPanel(left_header_, 10, 14, 142, 24, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(left_body_, 10, 39, 142, 160, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(right_header_, 168, 14, 142, 24, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(right_body_, 168, 42, 142, 158, GFX_TEXT_ALIGN_LEFT);
    // A full-width visual rail separates the real-time panels from TARS's
    // personality controls.  Repeated hyphens render as a 320 px console line
    // in the available 6 px bitmap font.
    ConfigureTextPanel(divider_, 0, 202, 320, 8, GFX_TEXT_ALIGN_CENTER);
    ConfigureTextPanel(footer_left_stroke_, 11, 216, 145, 22, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(footer_left_weight_, 10, 217, 145, 22, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(footer_left_, 10, 216, 145, 22, GFX_TEXT_ALIGN_LEFT);
    ConfigureTextPanel(footer_right_stroke_, 166, 216, 144, 22, GFX_TEXT_ALIGN_RIGHT);
    ConfigureTextPanel(footer_right_weight_, 165, 217, 144, 22, GFX_TEXT_ALIGN_RIGHT);
    ConfigureTextPanel(footer_right_, 165, 216, 144, 22, GFX_TEXT_ALIGN_RIGHT);
    auto* right_font = const_cast<lv_font_t*>(&lv_font_unscii_8);
    auto* left_font = const_cast<lv_font_t*>(&tars_log_font_6);
    gfx_label_set_font(left_header_, left_font);
    gfx_label_set_font(left_body_, left_font);
    gfx_label_set_font(right_header_, right_font);
    gfx_label_set_font(right_body_, right_font);
    gfx_label_set_font(divider_, left_font);
    auto* footer_font = const_cast<lv_font_t*>(&font_maison_neue_book_12);
    gfx_label_set_font(footer_left_stroke_, footer_font);
    gfx_label_set_font(footer_left_weight_, footer_font);
    gfx_label_set_font(footer_left_, footer_font);
    gfx_label_set_font(footer_right_stroke_, footer_font);
    gfx_label_set_font(footer_right_weight_, footer_font);
    gfx_label_set_font(footer_right_, footer_font);
    gfx_label_set_line_spacing(left_body_, 0);
    gfx_label_set_line_spacing(right_body_, 0);

    // Keep the provisioning EAF centred on TARS's 320x240 landscape panel.
    // This also configures the fallback object created above when an older
    // assets partition does not contain the corresponding layout entry.
    gfx_obj_align(wifi_config_animation_, GFX_ALIGN_CENTER, 0, 0);
    gfx_obj_set_size(wifi_config_animation_, 240, 240);
    gfx_obj_set_visible(wifi_config_animation_, false);

    // 标定失败图标：居中叠放在文本控制台上，默认隐藏
    BuildCalibErrorIcon();
    gfx_img_set_src(calib_error_icon_, &kCalibErrorIconDsc);
    gfx_obj_set_size(calib_error_icon_, kCalibErrorIconSize, kCalibErrorIconSize);
    gfx_obj_align(calib_error_icon_, GFX_ALIGN_CENTER, 0, 0);
    gfx_obj_set_visible(calib_error_icon_, false);

    // 标定失败原因：显示在屏幕最下面一行（覆盖 footer 区），默认隐藏。
    // 黑底遮住底部的 HONESTY/HUMOR 面板，红色文本居中显示错误原因。
    ConfigureTextPanel(calib_error_label_, 0, 214, 320, 26, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_font(calib_error_label_, footer_font);
    gfx_label_set_color(calib_error_label_, GFX_COLOR_HEX(kPanelRed));
    gfx_label_set_bg_enable(calib_error_label_, true);
    gfx_label_set_bg_color(calib_error_label_, GFX_COLOR_HEX(0x000000));
    gfx_obj_set_visible(calib_error_label_, false);
    emote_unlock(GetEmoteHandle());

    started_ = true;
    const esp_timer_create_args_t args = {
        .callback = &TarsDisplay::OnRefreshTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tars_status",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &refresh_timer_));
    // Keep the hardware telemetry (IMU, motor feedback, and battery) at a
    // stable one-second cadence; the flight log advances with the same tick.
    ESP_ERROR_CHECK(esp_timer_start_periodic(refresh_timer_, 1000000));
    Render();
}

void TarsDisplay::SetPersonality(int honesty, int humor) {
    honesty = honesty < 0 ? 0 : (honesty > 100 ? 100 : honesty);
    humor = humor < 0 ? 0 : (humor > 100 ? 100 : humor);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    honesty_ = honesty;
    humor_ = humor;
    const bool render_now = started_;
    xSemaphoreGive(mutex_);

    if (render_now) {
        Render();
    }
}

void TarsDisplay::OnRefreshTimer(void* arg) {
    auto* display = static_cast<TarsDisplay*>(arg);
    if (display == nullptr || !display->started_) {
        return;
    }

    xSemaphoreTake(display->mutex_, portMAX_DELAY);
    display->cursor_on_ = !display->cursor_on_;
    ++display->log_sequence_;
    xSemaphoreGive(display->mutex_);
    display->Render();
}

void TarsDisplay::ShowCalibrationError(const char* reason) {
    Begin();
    if (!started_ || calib_error_icon_ == nullptr) {
        return;
    }
    emote_lock(GetEmoteHandle());
    gfx_obj_set_visible(calib_error_icon_, true);
    if (calib_error_label_ != nullptr) {
        if (reason != nullptr && reason[0] != '\0') {
            gfx_label_set_text(calib_error_label_, reason);
        } else {
            gfx_label_set_text(calib_error_label_, "CALIBRATION FAILED");
        }
        gfx_obj_set_visible(calib_error_label_, true);
    }
    emote_unlock(GetEmoteHandle());
}

void TarsDisplay::HideCalibrationError() {
    if (calib_error_icon_ == nullptr) {
        return;
    }
    emote_lock(GetEmoteHandle());
    gfx_obj_set_visible(calib_error_icon_, false);
    if (calib_error_label_ != nullptr) {
        gfx_obj_set_visible(calib_error_label_, false);
    }
    emote_unlock(GetEmoteHandle());
}

void TarsDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr || emotion[0] == '\0') {
        return;
    }
    Begin();
    if (!started_) {
        return;
    }

    if (std::strcmp(emotion, "wificonfig") == 0) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        SetStateForEmotion(emotion);
        xSemaphoreGive(mutex_);
        ShowWifiConfigAnimation();
        return;
    }

    HideWifiConfigAnimation();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    SetStateForEmotion(emotion);
    xSemaphoreGive(mutex_);
    Render();
}

void TarsDisplay::SetConsoleVisible(bool visible) {
    gfx_obj_set_visible(left_header_, visible);
    gfx_obj_set_visible(left_body_, visible);
    gfx_obj_set_visible(right_header_, visible);
    gfx_obj_set_visible(right_body_, visible);
    gfx_obj_set_visible(divider_, visible);
    gfx_obj_set_visible(footer_left_stroke_, visible);
    gfx_obj_set_visible(footer_left_weight_, visible);
    gfx_obj_set_visible(footer_left_, visible);
    gfx_obj_set_visible(footer_right_stroke_, visible);
    gfx_obj_set_visible(footer_right_weight_, visible);
    gfx_obj_set_visible(footer_right_, visible);
}

void TarsDisplay::ShowWifiConfigAnimation() {
    // The Wi-Fi provisioning EAF contains the QR code.  Hide the console so
    // the code remains readable on the TARS panel, then select the matching
    // domestic or overseas wificonfig asset packaged for this firmware.
    emote_lock(GetEmoteHandle());
    SetConsoleVisible(false);
    emote_unlock(GetEmoteHandle());

    emote::EmoteDisplay::SetEmotion("wificonfig");
    wifi_config_animation_visible_ = true;
}

void TarsDisplay::HideWifiConfigAnimation() {
    if (!wifi_config_animation_visible_) {
        return;
    }

    emote_lock(GetEmoteHandle());
    gfx_obj_set_visible(wifi_config_animation_, false);
    SetConsoleVisible(true);
    emote_unlock(GetEmoteHandle());
    wifi_config_animation_visible_ = false;
}

void TarsDisplay::SetStatus(const char* status) {
    if (status == nullptr || status[0] == '\0') {
        return;
    }
    Begin();
    if (!started_) {
        return;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    notice_ = status;
    state_ = State::kExecuting;
    xSemaphoreGive(mutex_);
    Render();
}

void TarsDisplay::SetChatMessage(const char* role, const char* content) {
    (void)content;
    Begin();
    if (!started_) {
        return;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (role != nullptr && std::strcmp(role, "assistant") == 0) {
        state_ = State::kExecuting;
        notice_ = "VOICE RESPONSE";
    } else {
        state_ = State::kListening;
        notice_ = "VOICE INPUT";
    }
    xSemaphoreGive(mutex_);
    Render();
}

void TarsDisplay::ShowNotification(const char* notification, int duration_ms) {
    (void)duration_ms;
    if (notification == nullptr || notification[0] == '\0') {
        return;
    }
    Begin();
    if (!started_) {
        return;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    notice_ = notification;
    xSemaphoreGive(mutex_);
    Render();
}

void TarsDisplay::SetStateForEmotion(const char* emotion) {
    static constexpr const char* kListening[] = {"listen"};
    static constexpr const char* kAnalyzing[] = {"thinking"};
    static constexpr const char* kScanning[] = {"scanning"};
    static constexpr const char* kRemote[] = {"remote_mode"};
    static constexpr const char* kCalibration[] = {"calibration"};
    static constexpr const char* kWifi[] = {"wificonfig"};
    static constexpr const char* kReset[] = {"nvs_reset"};
    static constexpr const char* kSleep[] = {"sleepy", "icon_speaker_zzz"};
    static constexpr const char* kAlert[] = {"angry", "sad", "shocked", "surprised", "crying"};
    static constexpr const char* kSocial[] = {
        "happy", "silly", "winking", "cool", "laughing", "loving", "kiss",
        "confused", "embarrassed", "confident", "delicious", "relaxed"
    };

    if (std::strcmp(emotion, "launch") == 0) {
        state_ = State::kBoot;
        notice_ = "BOOT SEQUENCE";
    } else if (IsOneOf(emotion, kListening, sizeof(kListening) / sizeof(kListening[0]))) {
        state_ = State::kListening;
        notice_ = "INPUT ACQUIRED";
    } else if (IsOneOf(emotion, kAnalyzing, sizeof(kAnalyzing) / sizeof(kAnalyzing[0]))) {
        state_ = State::kAnalyzing;
        notice_ = "PARAMETER REVIEW";
    } else if (IsOneOf(emotion, kScanning, sizeof(kScanning) / sizeof(kScanning[0]))) {
        state_ = State::kScanning;
        notice_ = "ENVIRONMENT SCAN";
    } else if (IsOneOf(emotion, kRemote, sizeof(kRemote) / sizeof(kRemote[0]))) {
        state_ = State::kRemote;
        notice_ = "REMOTE LINK READY";
    } else if (IsOneOf(emotion, kCalibration, sizeof(kCalibration) / sizeof(kCalibration[0]))) {
        state_ = State::kCalibration;
        notice_ = "MOTION CALIBRATION";
    } else if (IsOneOf(emotion, kWifi, sizeof(kWifi) / sizeof(kWifi[0]))) {
        state_ = State::kWifi;
        notice_ = "NETWORK CONFIG";
    } else if (IsOneOf(emotion, kReset, sizeof(kReset) / sizeof(kReset[0]))) {
        state_ = State::kReset;
        notice_ = "RESET CONFIRM";
    } else if (IsOneOf(emotion, kSleep, sizeof(kSleep) / sizeof(kSleep[0]))) {
        state_ = State::kSleep;
        notice_ = "LOW POWER WATCH";
    } else if (IsOneOf(emotion, kAlert, sizeof(kAlert) / sizeof(kAlert[0]))) {
        state_ = State::kAlert;
        notice_ = "ATTENTION REQUIRED";
    } else if (IsOneOf(emotion, kSocial, sizeof(kSocial) / sizeof(kSocial[0]))) {
        state_ = State::kSocial;
        notice_ = "SOCIAL PROTOCOL";
    } else {
        state_ = State::kIdle;
        notice_ = "MISSION STANDBY";
    }
}

const char* TarsDisplay::StateName() const {
    switch (state_) {
        case State::kBoot: return "BOOT";
        case State::kIdle: return "STANDBY";
        case State::kListening: return "LISTENING";
        case State::kAnalyzing: return "THINKING";
        case State::kScanning: return "SCANNING";
        case State::kExecuting: return "SPEAKING";
        case State::kRemote: return "REMOTE";
        case State::kCalibration: return "CALIB";
        case State::kWifi: return "NETWORK";
        case State::kAlert: return "ALERT";
        case State::kReset: return "RESET";
        case State::kSocial: return "SOCIAL";
        case State::kSleep: return "LOW PWR";
    }
    return "STANDBY";
}

const char* TarsDisplay::StateDetail() const {
    switch (state_) {
        case State::kBoot: return "SYSTEM ONLINE";
        case State::kListening: return "AUDIO TRACK";
        case State::kAnalyzing: return "DATA LOCK";
        case State::kScanning: return "RANGE SWEEP";
        case State::kExecuting: return "TASK ACTIVE";
        case State::kRemote: return "BLE CONTROL";
        case State::kCalibration: return "AXIS CHECK";
        case State::kWifi: return "AWAITING LINK";
        case State::kAlert: return "CHECK SYSTEM";
        case State::kReset: return "HOLD TO RESET";
        case State::kSocial: return "HUMOR ENABLED";
        case State::kSleep: return "PASSIVE WATCH";
        case State::kIdle: return "MISSION READY";
    }
    return "MISSION READY";
}

void TarsDisplay::Render() {
    if (!started_ || mutex_ == nullptr) {
        return;
    }

    char left[512] = {};
    char right[224] = {};
    char footer_left[48] = {};
    char footer_right[48] = {};
    uint32_t system_color = kSystemGreen;

    uint32_t log_sequence = 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    log_sequence = log_sequence_;
    const char* state_name = StateName();
    const char* state_detail = StateDetail();
    std::snprintf(footer_left, sizeof(footer_left), "HONESTY %d%%", honesty_);
    std::snprintf(footer_right, sizeof(footer_right), "HUMOR %d%%", humor_);
    if (state_ == State::kAlert || state_ == State::kReset) {
        system_color = state_ == State::kAlert ? kPanelRed : kPanelAmber;
    } else if (state_ == State::kBoot || state_ == State::kCalibration) {
        system_color = kPanelAmber;
    }
    xSemaphoreGive(mutex_);

    size_t left_used = 0;
    constexpr size_t kLogCount = sizeof(kFlightLog) / sizeof(kFlightLog[0]);
    // The 6 px font fits 23 records in the full-height log pane, giving the
    // left channel its intentionally dense, continuously flowing treatment.
    for (int row = 22; row >= 0; --row) {
        const uint32_t event_id = log_sequence >= static_cast<uint32_t>(row)
            ? log_sequence - row : 0;
        left_used += std::snprintf(left + left_used, sizeof(left) - left_used,
                                   "%04lu %s\n", static_cast<unsigned long>(event_id),
                                   kFlightLog[event_id % kLogCount]);
    }
    const float motor_1 = static_cast<float>(motor[0].FbPos - motor[0].ZeroPos) * M_A / M_N;
    const float motor_2 = static_cast<float>(motor[1].FbPos - motor[1].ZeroPos) * M_A / M_N;
    const float motor_3 = static_cast<float>(motor[2].FbPos - motor[2].ZeroPos) * M_A / M_N;
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    const bool battery_valid = Board::GetInstance().GetBatteryLevel(
        battery_level, charging, discharging);
    const char* imu_state = !imu_is_initialized() ? "OFF" : (isIMUInit ? "OK" : "CAL");
    std::snprintf(right, sizeof(right),
                  "ST %s\nBAT %3d%%\nV %4.1f\nM1 %5.0f\nM2 %5.0f\nM3 %5.0f\n"
                  "IMU %s\nR %5.1f\nP %5.1f\nY %5.1f\n"
                  "AX %4.1f\nAY %4.1f\nAZ %4.1f\nNET %s\n%s",
                  state_name, battery_valid ? battery_level : 0, servo_voltage,
                  motor_1, motor_2, motor_3, imu_state, roll, pitch, yaw,
                  accel_x, accel_y, accel_z,
                  charging ? "CHG" : (discharging ? "ON" : "WAIT"), state_detail);

    emote_lock(GetEmoteHandle());
    gfx_label_set_text(left_header_, "[ FLIGHT LOG ]");
    gfx_label_set_text(left_body_, left);
    gfx_label_set_text(right_header_, "[ TARS / STATUS ]");
    gfx_label_set_text(right_body_, right);
    gfx_label_set_text(divider_, "--------------------------------------------------------------------------------");
    gfx_label_set_text(footer_left_stroke_, footer_left);
    gfx_label_set_text(footer_left_weight_, footer_left);
    gfx_label_set_text(footer_left_, footer_left);
    gfx_label_set_text(footer_right_stroke_, footer_right);
    gfx_label_set_text(footer_right_weight_, footer_right);
    gfx_label_set_text(footer_right_, footer_right);
    gfx_label_set_color(left_header_, GFX_COLOR_HEX(kLogBlue));
    gfx_label_set_color(left_body_, GFX_COLOR_HEX(kLogBlueDim));
    gfx_label_set_color(right_header_, GFX_COLOR_HEX(system_color));
    gfx_label_set_color(right_body_, GFX_COLOR_HEX(
        system_color == kSystemGreen ? kSystemGreenDim : system_color));
    gfx_label_set_color(divider_, GFX_COLOR_HEX(kLogBlueDim));
    gfx_label_set_color(footer_left_stroke_, GFX_COLOR_HEX(kLogBlue));
    gfx_label_set_color(footer_left_weight_, GFX_COLOR_HEX(kLogBlue));
    gfx_label_set_color(footer_left_, GFX_COLOR_HEX(kLogBlue));
    gfx_label_set_color(footer_right_stroke_, GFX_COLOR_HEX(kSystemGreen));
    gfx_label_set_color(footer_right_weight_, GFX_COLOR_HEX(kSystemGreen));
    gfx_label_set_color(footer_right_, GFX_COLOR_HEX(kSystemGreen));
    emote_unlock(GetEmoteHandle());
}
