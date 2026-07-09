#include "st7565r_display.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "St7565rDisplay";

// LVGL v9 刷屏回调：在这里把彩色转成黑白，并推送给屏幕
static void st7565r_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    St7565rDisplay* display = (St7565rDisplay*)lv_display_get_user_data(disp);
    St7565r* lcd = display->GetHardware();

    // px_map 是 LVGL 传来的彩色数据 (RGB565, 每个像素 2 字节)
    uint16_t* rgb_map = (uint16_t*)px_map;

    // 遍历当前需要刷新的区域
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint16_t color = *rgb_map++;
            
            // 提取 RGB 值并计算亮度 (灰度)
            uint8_t r = (color >> 11) & 0x1F;
            uint8_t g = (color >> 5) & 0x3F;
            uint8_t b = color & 0x1F;
            // 简单的灰度换算
            int brightness = (r * 8) + (g * 4) + (b * 8); 
            
            // 亮度过半则为白(点亮)，否则为黑(熄灭)
            lcd->DrawPixel(x, y, brightness > 128 ? 1 : 0);
        }
    }

    // 转换完毕，通知底层硬件推屏
    lcd->Flush();
    
    // 通知 LVGL v9 刷屏完成
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

// 补齐的 Lock/Unlock 实现
bool St7565rDisplay::Lock(int timeout_ms) {
    // 如果系统没有要求特定的超时逻辑，直接 try_lock 即可
    return mutex_.try_lock();
}

void St7565rDisplay::Unlock() {
    mutex_.unlock();
}

void St7565rDisplay::InitializeLvgl() {
    // 【最关键的一行】：初始化 LVGL 的内部系统和内存池(TLSF)
    lv_init(); 

    // 现在调用就不会导致 TLSF 崩溃了
    disp_ = lv_display_create(width_, height_);
    
    int buf_size = width_ * height_ * 2;
    
    // 使用默认或内部 RAM 分配
    lv_buffer_ = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (lv_buffer_ == nullptr) {
        lv_buffer_ = (uint8_t*)malloc(buf_size);
    }

    if (lv_buffer_ == nullptr) {
        ESP_LOGE("St7565rDisplay", "LVGL buffer alloc failed!");
        return; 
    }
    
    lv_display_set_buffers(disp_, lv_buffer_, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp_, st7565r_flush_cb);
    lv_display_set_user_data(disp_, this);
}

void St7565rDisplay::SetupUI() {
    lv_obj_t* scr = lv_screen_active(); // v9 API
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    status_label_ = lv_label_create(scr);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_white(), 0);
    lv_label_set_text(status_label_, "Booting...");

    chat_label_ = lv_label_create(scr);
    lv_obj_align(chat_label_, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_text_color(chat_label_, lv_color_white(), 0);
    lv_label_set_long_mode(chat_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(chat_label_, width_ - 4);
    lv_label_set_text(chat_label_, "");
}

void St7565rDisplay::SetChatMessage(const char* role, const char* content) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(chat_label_) {
        lv_label_set_text_fmt(chat_label_, "%s: %s", role ? role : "", content ? content : "");
    }
}

void St7565rDisplay::SetEmotion(const char* emotion) {
    // 单色屏表情逻辑
}

// 如果 display.h 中有 SetStatus(const char*), 请在这里补上实现
// void St7565rDisplay::SetStatus(const char* status) { ... }