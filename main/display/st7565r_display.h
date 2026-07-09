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

    // 1. 补齐系统要求的 Lock / Unlock (解决 abstract class 报错)
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    // 2. 注意：请打开你的 main/display/display.h 看看下面这几个函数的确切参数
    // 如果 display.h 里没有它们，就直接删掉；如果有，请保持参数类型完全一致。
    // virtual void SetStatus(const char* status) override; // 可能改成了 const char*
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    // virtual void SetIcon(const char* icon) override;

    St7565r* GetHardware() { return hw_lcd_; }

private:
    void InitializeLvgl();
    void SetupUI();

    St7565r* hw_lcd_;
    int width_;
    int height_;
    
    // LVGL v9 类型
    lv_display_t* disp_;
    uint8_t* lv_buffer_; // 用于存放 LVGL 的 RGB565 渲染数据

    lv_obj_t* status_label_;
    lv_obj_t* chat_label_;
    
    std::mutex mutex_;
};

#endif // _ST7565R_DISPLAY_H_