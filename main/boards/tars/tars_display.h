#pragma once

#include "display/emote_display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>

// TARS does not use a character face.  It keeps the emote asset runtime for
// wake-word assets, but renders its interaction state as two independent
// mission consoles on the landscape panel.
class TarsDisplay final : public emote::EmoteDisplay {
public:
    TarsDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io, int width, int height);
    ~TarsDisplay() override;

    // Must be called after Assets::Apply(), when the layout labels exist.
    void Begin();

    // The personality controls are displayed in the two independent footer
    // panels.  They may be set before Begin(); Begin() will render them once
    // the expression runtime is ready.
    void SetPersonality(int honesty, int humor);

    void SetEmotion(const char* emotion) override;
    void SetStatus(const char* status) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;

    // 标定零位写入失败时的屏幕提示：红色圆环 + 红色感叹号图标，
    // reason 非空时同时显示在屏幕最下面一行
    void ShowCalibrationError(const char* reason = nullptr);
    void HideCalibrationError();

private:
    enum class State {
        kBoot,
        kIdle,
        kListening,
        kAnalyzing,
        kScanning,
        kExecuting,
        kRemote,
        kCalibration,
        kWifi,
        kAlert,
        kReset,
        kSocial,
        kSleep,
    };

    static void OnRefreshTimer(void* arg);
    void Render();
    void SetStateForEmotion(const char* emotion);
    void SetConsoleVisible(bool visible);
    void ShowWifiConfigAnimation();
    void HideWifiConfigAnimation();
    const char* StateName() const;
    const char* StateDetail() const;

    gfx_obj_t* left_header_ = nullptr;
    gfx_obj_t* left_body_ = nullptr;
    gfx_obj_t* right_header_ = nullptr;
    gfx_obj_t* right_body_ = nullptr;
    gfx_obj_t* divider_ = nullptr;
    gfx_obj_t* footer_left_ = nullptr;
    gfx_obj_t* footer_left_weight_ = nullptr;
    gfx_obj_t* footer_left_stroke_ = nullptr;
    gfx_obj_t* footer_right_ = nullptr;
    gfx_obj_t* footer_right_weight_ = nullptr;
    gfx_obj_t* footer_right_stroke_ = nullptr;
    gfx_obj_t* wifi_config_animation_ = nullptr;
    gfx_obj_t* calib_error_icon_ = nullptr;
    gfx_obj_t* calib_error_label_ = nullptr;
    esp_timer_handle_t refresh_timer_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    State state_ = State::kBoot;
    std::string notice_ = "BOOT SEQUENCE";
    uint32_t log_sequence_ = 0;
    bool cursor_on_ = false;
    bool started_ = false;
    bool wifi_config_animation_visible_ = false;
    int honesty_ = 90;
    int humor_ = 75;
};
