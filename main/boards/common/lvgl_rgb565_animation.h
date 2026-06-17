#pragma once

#include <esp_log.h>
#include <lvgl.h>
#include "display/lvgl_display/lvgl_image.h"

/**
 * Pre-decoded RGB565 animation player
 * Zero runtime decode - just memory copy
 */
class LvglRgb565Animation : public LvglImage {
public:
    struct FrameData {
        uint16_t width;
        uint16_t height;
        uint16_t frame_count;
        uint16_t frame_delay_ms;
        uint32_t frame_size;
        const uint8_t* frames;
    };

    LvglRgb565Animation(const FrameData& data) 
        : data_(data), img_obj_(nullptr), current_frame_(0), playing_(false), timer_(nullptr) {
        
        // Create LVGL image descriptor
        img_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        img_dsc_.header.w = data_.width;
        img_dsc_.header.h = data_.height;
        img_dsc_.data_size = data_.frame_size;
        img_dsc_.data = data_.frames;  // Start with first frame
        
        ESP_LOGI("RGB565Anim", "Created: %dx%d, %d frames, %dms delay", 
                 data_.width, data_.height, data_.frame_count, data_.frame_delay_ms);
    }
    
    virtual ~LvglRgb565Animation() {
        if (timer_) {
            lv_timer_delete(timer_);
        }
    }
    
    // LvglImage interface
    const lv_img_dsc_t* image_dsc() const override {
        return &img_dsc_;
    }
    
    bool IsGif() const override { return true; }  // Treat as animated
    
    // Animation control
    void SetImageObj(lv_obj_t* obj) {
        img_obj_ = obj;
    }
    
    void Start() {
        if (!timer_) {
            timer_ = lv_timer_create([](lv_timer_t* t) {
                auto* self = static_cast<LvglRgb565Animation*>(lv_timer_get_user_data(t));
                self->NextFrame();
            }, data_.frame_delay_ms, this);
        }
        playing_ = true;
        current_frame_ = 0;
        lv_timer_resume(timer_);
        lv_timer_reset(timer_);
    }
    
    void Pause() {
        playing_ = false;
        if (timer_) {
            lv_timer_pause(timer_);
        }
    }
    
    void Stop() {
        Pause();
        current_frame_ = 0;
        UpdateFrame();
    }

private:
    void NextFrame() {
        if (!playing_) return;
        
        current_frame_ = (current_frame_ + 1) % data_.frame_count;
        UpdateFrame();
    }
    
    void UpdateFrame() {
        // Point to current frame data
        img_dsc_.data = data_.frames + (current_frame_ * data_.frame_size);
        
        // Invalidate the image to trigger redraw
        if (img_obj_) {
            lv_obj_invalidate(img_obj_);
        }
    }
    
    FrameData data_;
    mutable lv_img_dsc_t img_dsc_ = {};
    lv_obj_t* img_obj_;
    uint32_t current_frame_;
    bool playing_;
    lv_timer_t* timer_;
};
