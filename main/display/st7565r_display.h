#ifndef _ST7565R_DISPLAY_H_
#define _ST7565R_DISPLAY_H_

#include "display.h"
#include "st7565r.h"
#include "lvgl.h"
#include <string>
#include <mutex>

class St7565rDisplay : public Display {
public:
    St7565rDisplay(St7565r* hw_lcd, int width, int height);
    virtual ~St7565rDisplay();

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    // 根据小智系统的标准虚函数补齐
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;

    St7565r* GetHardware() { return hw_lcd_; }

private:
    void InitializeLvgl();
    void SetupUI();

    St7565r* hw_lcd_;
    int width_;
    int height_;
    
    lv_display_t* disp_;
    uint8_t* lv_buffer_; 

    lv_obj_t* status_label_;
    lv_obj_t* chat_label_;
    
    std::mutex mutex_;
};

#endif // _ST7565R_DISPLAY_H_