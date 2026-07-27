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
// 图标位图数据 (10x10 单色像素) 
// --------------------------------------------------------
static const unsigned char ICON_USER_10[] = {
    0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x30, 0x00, 0x00, 0x00, 
    0xFC, 0x00, 0xFE, 0x01, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03
};

static const unsigned char ICON_BOT_10[] = {
    0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0xFE, 0x01, 0x85, 0x02, 
    0x01, 0x02, 0xFD, 0x02, 0x01, 0x02, 0xFE, 0x01, 0x00, 0x00
};

static const unsigned char ICON_SYS_10[] = {
    0x78, 0x00, 0xCC, 0x00, 0xB6, 0x01, 0x7B, 0x03, 0xFD, 0x02, 
    0xFD, 0x02, 0x7B, 0x03, 0xB6, 0x01, 0xCC, 0x00, 0x78, 0x00
};

enum RoleType {
    ROLE_TYPE_ASSISTANT,
    ROLE_TYPE_USER,
    ROLE_TYPE_SYSTEM
};

// --------------------------------------------------------
// 1. 定义你的 ST7565 单色屏驱动类，继承小智的 Display 接口
// --------------------------------------------------------
class Mini12864Display : public Display {
private:
    u8g2_t u8g2;
    char current_msg[512] = "Xiaozhi Ready!";
    char current_status[64] = "初始化中...";   
    RoleType current_role = ROLE_TYPE_ASSISTANT; 
    SemaphoreHandle_t mutex;

    // 屏幕实时刷新任务
    static void display_task(void* pvParameter) {
        Mini12864Display* display = (Mini12864Display*)pvParameter;

        int rssi = -100;
        int wifi_check_counter = 0;

        while (1) {
            if (xSemaphoreTake(display->mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                
                // 【修复核心1】：彻底移除所有的 u8g2_SetClipWindow，让清屏指令 100% 覆盖整个显存，告别雪花噪点！
                u8g2_ClearBuffer(&display->u8g2);

                // ==========================================
                // 1. 顶部状态栏绘制 (Y: 0 ~ 14)
                // ==========================================
                u8g2_SetFont(&display->u8g2, u8g2_font_wqy12_t_gb2312);
                
                const unsigned char* title_icon = ICON_BOT_10;
                if (display->current_role == ROLE_TYPE_USER) title_icon = ICON_USER_10;
                else if (display->current_role == ROLE_TYPE_SYSTEM) title_icon = ICON_SYS_10;
                
                u8g2_DrawXBM(&display->u8g2, 0, 2, 10, 10, title_icon);
                u8g2_DrawUTF8(&display->u8g2, 14, 11, display->current_status);

                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);
                char time_str[16];
                if (timeinfo.tm_year < (2023 - 1900)) {
                    snprintf(time_str, sizeof(time_str), "--:--");
                } else {
                    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                }
                u8g2_DrawStr(&display->u8g2, 86, 10, time_str);

                if (wifi_check_counter++ % 30 == 0) {
                    wifi_ap_record_t ap_info;
                    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) rssi = ap_info.rssi;
                    else rssi = -100;
                }
                int bars = 0;
                if (rssi > -60) bars = 4;
                else if (rssi > -70) bars = 3;
                else if (rssi > -80) bars = 2;
                else if (rssi > -90) bars = 1;

                for (int i = 0; i < 4; i++) {
                    int bar_h = 2 + i * 2;
                    int bar_x = 114 + i * 3;
                    int bar_y = 11 - bar_h;
                    if (i < bars) u8g2_DrawBox(&display->u8g2, bar_x, bar_y, 2, bar_h);
                    else u8g2_DrawFrame(&display->u8g2, bar_x, bar_y, 2, bar_h);
                }

                u8g2_DrawHLine(&display->u8g2, 0, 14, 128);

                // ==========================================
                // 2. 底部对话交互区 (使用最安全的二维数组缓冲，防越界奔溃)
                // ==========================================
                u8g2_SetFont(&display->u8g2, u8g2_font_wqy12_t_gb2312);
                
                // 静态分配缓冲：最多 10 行，每行最多存 36 个字符空间
                char line_buffers[10][36]; 
                int total_lines = 0;
                char* p = display->current_msg;

                // 【修复核心2】：安全文本切分算法
                while (*p != '\0' && total_lines < 10) {
                    int len = 0;
                    
                    // 往前探测，最多取 30 个字节
                    while (len < 30 && p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
                        len++;
                    }
                    
                    // 如果刚好切在 30 字节上限，检查是否切碎了 UTF-8 中文
                    if (len == 30 && p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
                        while (len > 0 && (p[len] & 0xC0) == 0x80) {
                            len--; // 是延续字节就回退，确保字是完整的
                        }
                    }
                    
                    // 将完整的这一行存入安全的二维数组
                    if (len > 0) {
                        memset(line_buffers[total_lines], 0, sizeof(line_buffers[total_lines])); // 清零
                        strncpy(line_buffers[total_lines], p, len);
                        total_lines++;
                        p += len;
                    } else {
                        p++; // 如果 len=0，通常是遇到了 \n，往前推一格
                    }
                    
                    // 跳过所有的连续换行符
                    while (*p == '\r' || *p == '\n') p++;
                }

                // 【修复核心3】：极简翻页逻辑
                int max_lines_per_page = 3;
                int start_line = 0;
                if (total_lines > 0) {
                    // 当 total_lines = 4 时，(4-1)/3*3 = 3，从第三行索引（即第四句话）开始画
                    start_line = ((total_lines - 1) / max_lines_per_page) * max_lines_per_page;
                }

                // 绘制当前页的文本，Y坐标从 28 开始，字体最高12px，刚好完美避开 Y=14 的横线
                int y = 28;  
                for (int i = start_line; i < total_lines && i < start_line + max_lines_per_page; i++) {
                    u8g2_DrawUTF8(&display->u8g2, 0, y, line_buffers[i]);
                    y += 14;     
                }

                u8g2_SendBuffer(&display->u8g2);
                xSemaphoreGive(display->mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(100));  
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

        u8g2_Setup_st7565_jlx12864_f(
            &u8g2,                        
            U8G2_R0,                      
            u8g2_esp32_spi_byte_cb,       
            u8g2_esp32_gpio_and_delay_cb  
        );

        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetContrast(&u8g2, 66);

        xTaskCreate(display_task, "lcd_task", 4096, this, 2, NULL);
    }

    ~Mini12864Display() {
        if (mutex) vSemaphoreDelete(mutex);
    }

    virtual void SetStatus(const char* status) override {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (status == NULL || strlen(status) == 0) {
                current_status[0] = '\0';
                xSemaphoreGive(mutex);
                return;
            }

            const char* p = status;
            
            // 精准切除 "[gear]" 这类烦人的英文前缀
            const char* last_bracket = strrchr(p, ']');
            if (last_bracket != NULL) {
                p = last_bracket + 1;
            } else {
                const char* newline_pos = strrchr(p, '\n');
                if (newline_pos != NULL) p = newline_pos + 1;
            }

            while (*p == ' ' || *p == '\r' || *p == '\n') p++;

            if (*p == '\0') p = status; 

            int j = 0;
            for (int i = 0; p[i] != '\0' && j < sizeof(current_status) - 1; i++) {
                if (p[i] == '[') { 
                    while (p[i] != '\0' && p[i] != ']') i++;
                    continue;
                }
                if ((unsigned char)p[i] >= 32) { // 只保留正常的可见字符
                    current_status[j++] = p[i];
                }
            }
            current_status[j] = '\0';

            // 去掉尾巴上的多余空格
            while (j > 0 && current_status[j - 1] == ' ') {
                current_status[--j] = '\0';
            }

            xSemaphoreGive(mutex);
        }
    }

    virtual void SetChatMessage(const char* role, const char* content) override {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            if (role != NULL) {
                if (strcasecmp(role, "user") == 0) current_role = ROLE_TYPE_USER;
                else if (strcasecmp(role, "system") == 0) current_role = ROLE_TYPE_SYSTEM;
                else current_role = ROLE_TYPE_ASSISTANT;
            } else {
                current_role = ROLE_TYPE_ASSISTANT;
            }

            const char* raw = content ? content : "";
            while (*raw == '\r' || *raw == '\n' || *raw == ' ') raw++;

            int j = 0;
            bool in_bracket = false;
            for (int i = 0; raw[i] != '\0' && j < sizeof(current_msg) - 1; i++) {
                if (raw[i] == '[') {
                    in_bracket = true;
                    continue;
                }
                if (raw[i] == ']') {
                    in_bracket = false;
                    if (raw[i + 1] == ' ') i++; 
                    continue;
                }
                if (in_bracket) continue;

                if ((unsigned char)raw[i] < 32 && raw[i] != '\n' && raw[i] != '\r') continue;
                
                current_msg[j++] = raw[i];
            }
            current_msg[j] = '\0';

            if (strlen(current_msg) == 0) {
                snprintf(current_msg, sizeof(current_msg), "...");
            }

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

    virtual Display* GetDisplay() override { return display_; }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec( 
            AUDIO_INPUT_SAMPLE_RATE,            
            AUDIO_OUTPUT_SAMPLE_RATE,           
            AUDIO_I2S_GPIO_BCLK,                
            AUDIO_I2S_GPIO_WS,                  
            AUDIO_I2S_GPIO_DOUT,                
            AUDIO_I2S_GPIO_DIN                  
        );
        return &audio_codec;
    }
};

DECLARE_BOARD(MyC3Board);