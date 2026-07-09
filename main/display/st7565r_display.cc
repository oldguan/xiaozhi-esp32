#include "st7565r_display.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char* TAG = "St7565rDisplay";

static void st7565r_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    St7565rDisplay* display = (St7565rDisplay*)lv_display_get_user_data(disp);
    St7565r* lcd = display->GetHardware();

    uint16_t* rgb_map = (uint16_t*)px_map;

    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint16_t color = *rgb_map++;
            
            uint8_t r = (color >> 11) & 0x1F;
            uint8_t g = (color >> 5) & 0x3F;
            uint8_t b = color & 0x1F;
            int brightness = (r * 8) + (g * 4) + (b * 8); 
            
            // 【关键翻转逻辑】：
            // 如果亮度 > 128 (浅色/背景)，写入 0 (不发光/透明)
            // 如果亮度 <= 128 (深色/文字)，写入 1 (发光/变黑)
            lcd->DrawPixel(x, y, brightness > 128 ? 0 : 1);
        }
    }

    lcd->Flush();
    lv_display_flush_ready(disp);
}

St7565rDisplay::St7565rDisplay(St7565r* hw_lcd, int width, int height) 
    : hw_lcd_(hw_lcd), width_(width), height_(height) {
    InitializeLvgl();
    SetupUI();
}

St7565rDisplay::~St7565rDisplay() {
    free(lv_buffer_);
}

bool St7565rDisplay::Lock(int timeout_ms) {
    return mutex_.try_lock();
}

void St7565rDisplay::Unlock() {
    mutex_.unlock();
}

void St7565rDisplay::InitializeLvgl() {
    ESP_LOGI(TAG, "Init LVGL Core");
    lv_init(); // 必须调用，防止 TLSF 崩溃

    disp_ = lv_display_create(width_, height_);
    
    int buf_size = width_ * height_ * 2;
    lv_buffer_ = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (lv_buffer_ == nullptr) {
        lv_buffer_ = (uint8_t*)malloc(buf_size);
    }

    if (lv_buffer_ == nullptr) {
        ESP_LOGE(TAG, "LVGL buffer alloc failed!");
        return; 
    }
    
    lv_display_set_buffers(disp_, lv_buffer_, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp_, st7565r_flush_cb);
    lv_display_set_user_data(disp_, this);
}

void St7565rDisplay::SetupUI() {
    lv_obj_t* scr = lv_screen_active();
    // 单色屏 UI，背景设为浅色(白)，文字设为深色(黑)
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    status_label_ = lv_label_create(scr);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);
    lv_label_set_text(status_label_, "Booting...");

    chat_label_ = lv_label_create(scr);
    lv_obj_align(chat_label_, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_text_color(chat_label_, lv_color_black(), 0);
    lv_label_set_long_mode(chat_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(chat_label_, width_ - 4);
    lv_label_set_text(chat_label_, "");
}

void St7565rDisplay::SetStatus(const char* status) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(status_label_ && status != nullptr) { // 加上判空保护
        lv_label_set_text(status_label_, status);
    }
}

void St7565rDisplay::SetChatMessage(const char* role, const char* content) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(chat_label_) {
        lv_label_set_text_fmt(chat_label_, "%s: %s", role ? role : "", content ? content : "");
    }
}

void St7565rDisplay::SetEmotion(const char* emotion) {}
 