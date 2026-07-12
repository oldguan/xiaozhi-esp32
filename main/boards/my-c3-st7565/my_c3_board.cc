#include <time.h>
#include "audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display.h"
#include "esp_wifi.h"
#include "wifi_board.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

extern "C" {
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
}
// --------------------------------------------------------
// 1. 定义你的 ST7565 单色屏驱动类，继承小智的 Display 接口
// --------------------------------------------------------
class Mini12864Display : public Display {
private:
    u8g2_t u8g2;
    char current_msg[512] = "Xiaozhi Ready!";  // 【修改1】扩容到 512 字节，支持多行长文本
    SemaphoreHandle_t mutex;

    // 【修改2】升级版辅助函数：计算下一行的字节长度
    // 既能检测 \n 主动换行，又能在达到显示宽度上限 (max_bytes) 时自动折行，且绝不拆散 UTF-8 汉字！
    static int get_next_line_len(const char* str, int max_bytes) {
        int len = 0;
        // 遇到字符串结束符 \0、换行符 \n 或 \r 时立即停止，实现主动换行
        while (len < max_bytes && str[len] != '\0' && str[len] != '\n' && str[len] != '\r') {
            len++;
        }
        // 如果是因为达到了 max_bytes 宽度上限而停止，需向后倒退到 UTF-8 字符首字节，防止割裂汉字
        if (len == max_bytes && str[len] != '\0' && str[len] != '\n' && str[len] != '\r') {
            while (len > 0 && (str[len] & 0xC0) == 0x80) {
                len--;
            }
        }
        return len;
    }

    // 屏幕实时刷新任务 (防花屏 & 支持 \n 换行安全版)
    static void display_task(void* pvParameter) {
        Mini12864Display* display = (Mini12864Display*)pvParameter;

        int rssi = -100;
        int wifi_check_counter = 0;

        while (1) {
            if (xSemaphoreTake(display->mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                u8g2_ClearBuffer(&display->u8g2);

                // ==========================================
                // 1. 顶部状态栏 (Y: 0 ~ 14)
                // ==========================================
                u8g2_SetFont(&display->u8g2, u8g2_font_wqy12_t_gb2312);
                u8g2_DrawStr(&display->u8g2, 2, 10, "Xiaozhi");

                // --- 实时时间 ---
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);
                char time_str[16];
                if (timeinfo.tm_year < (2023 - 1900)) {
                    snprintf(time_str, sizeof(time_str), "--:--");
                } else {
                    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour,
                             timeinfo.tm_min);
                }
                u8g2_DrawStr(&display->u8g2, 86, 10, time_str);

                // --- 4格 Wi-Fi 信号柱 ---
                if (wifi_check_counter++ % 30 == 0) {
                    wifi_ap_record_t ap_info;
                    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                        rssi = ap_info.rssi;
                    } else {
                        rssi = -100;
                    }
                }

                int bars = 0;
                if (rssi > -60)
                    bars = 4;
                else if (rssi > -70)
                    bars = 3;
                else if (rssi > -80)
                    bars = 2;
                else if (rssi > -90)
                    bars = 1;

                for (int i = 0; i < 4; i++) {
                    int bar_h = 2 + i * 2;
                    int bar_x = 114 + i * 3;
                    int bar_y = 11 - bar_h;
                    if (i < bars) {
                        u8g2_DrawBox(&display->u8g2, bar_x, bar_y, 2, bar_h);
                    } else {
                        u8g2_DrawFrame(&display->u8g2, bar_x, bar_y, 2, bar_h);
                    }
                }

                u8g2_DrawHLine(&display->u8g2, 0, 14, 128);

                // ==========================================
                // 2. 底部对话交互区 (支持 \n 主动换行 + 达到宽度自动折行)
                // ==========================================
                u8g2_SetFont(&display->u8g2, u8g2_font_wqy12_t_gb2312);
                char* ptr = display->current_msg;
                int y = 28;         // 第一行的基线 Y 坐标
                int max_lines = 3;  // 128x64 屏幕扣除状态栏后，底部最多展示 3 行文字

                // 【修改3】重构为循环绘制，自动适配 \n 换行和长文本折行
                for (int i = 0; i < max_lines && *ptr != '\0'; i++) {
                    // 每行最多容纳 30 个 UTF-8 字节 (10个中文汉字或30个英文字母)
                    int len = get_next_line_len(ptr, 30);
                    if (len > 0) {
                        char line_buf[36] = {0};
                        strncpy(line_buf, ptr, len);
                        u8g2_DrawUTF8(&display->u8g2, 0, y, line_buf);
                    }
                    y += 14;     // 下移一行 (行高 14px)
                    ptr += len;  // 指针移动到当前行末尾

                    // 跳过末尾所有的换行符 \r 和 \n，让下一行文字从干净的字符开始
                    while (*ptr == '\r' || *ptr == '\n') {
                        ptr++;
                    }
                }

                u8g2_SendBuffer(&display->u8g2);
                xSemaphoreGive(display->mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // 维持稳健的 10FPS 刷新率
        }
    }

public:
    Mini12864Display() {
        mutex = xSemaphoreCreateMutex();

        u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
        u8g2_esp32_hal.clk = DISPLAY_SCLK_PIN;
        u8g2_esp32_hal.mosi = DISPLAY_MOSI_PIN;
        u8g2_esp32_hal.cs = DISPLAY_CS_PIN;
        u8g2_esp32_hal.dc = DISPLAY_DC_PIN;
        u8g2_esp32_hal.reset = DISPLAY_RST_PIN;
        u8g2_esp32_hal_init(u8g2_esp32_hal);

        u8g2_Setup_st7565_jlx12864_f(     //
            &u8g2,                        //
            U8G2_R0,                      //
            u8g2_esp32_spi_byte_cb,       // 硬件 SPI 数据传输回调
            u8g2_esp32_gpio_and_delay_cb  // 硬件延时与 GPIO 控制回调
        );

        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetContrast(&u8g2, 66);

        xTaskCreate(display_task, "lcd_task", 4096, this, 2, NULL);
    }

    ~Mini12864Display() {
        if (mutex)
            vSemaphoreDelete(mutex);
    }

    virtual void SetChatMessage(const char* role, const char* content) override {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 【修改4】在 role 后添加
            // \n，这样角色名（如“小智:”）会占第一行，AI的回复内容会自动从第二行开始显示
            snprintf(current_msg, sizeof(current_msg), "%s:\n%s", role, content);
            xSemaphoreGive(mutex);
        }
    }

    virtual bool Lock(int timeout_ms = 0) override {
        return xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    }

    virtual void Unlock() override { xSemaphoreGive(mutex); }
};

// --------------------------------------------------------
// 定义开发板类，继承 WifiBoard
// --------------------------------------------------------
class MyC3Board : public WifiBoard {
private:
    Display* display_;

public:
    MyC3Board() { display_ = new Mini12864Display(); }

    // 核心：系统要找屏幕，我们就把自己的单色屏交出去
    virtual Display* GetDisplay() override { return display_; }

    // 使用 NoAudioCodec 对接 MAX98357A 和 INMP441
    virtual AudioCodec* GetAudioCodec() override {
        // 明确传入采样率和引脚
        static NoAudioCodecDuplex audio_codec(  //
            AUDIO_INPUT_SAMPLE_RATE,            // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,           // 输出采样率
            AUDIO_I2S_GPIO_BCLK,                // BCLK
            AUDIO_I2S_GPIO_WS,                  // LRCK
            AUDIO_I2S_GPIO_DOUT,                // MAX98357 DIN
            AUDIO_I2S_GPIO_DIN                  // INMP441 SD
        );
        return &audio_codec;
    }
};

// --------------------------------------------------------
// 注册你的开发板
// --------------------------------------------------------
DECLARE_BOARD(MyC3Board);