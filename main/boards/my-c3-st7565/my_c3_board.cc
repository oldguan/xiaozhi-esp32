#include "audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display.h"
#include "display/st7565r.h"
#include "display/st7565r_display.h"
#include "wifi_board.h"

// 引入你拉取到 components 里的 U8g2 库
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// 修改后：
extern "C" {
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
}

// --------------------------------------------------------
// 1. 定义你的 ST7565 单色屏驱动类，继承小智的 Display 接口
// --------------------------------------------------------
class Mini12864Display : public Display {
private:
    u8g2_t u8g2;
    char current_msg[64] = "Xiaozhi Starting...";  // 缓存当前系统要显示的文本

    // 屏幕刷新任务（跑在后台，替代 Arduino 的 loop）
    static void display_task(void* pvParameter) {
        Mini12864Display* display = (Mini12864Display*)pvParameter;
        while (1) {
            u8g2_ClearBuffer(&display->u8g2);
            u8g2_SetFont(&display->u8g2, u8g2_font_ncenB08_tr);

            // 简单画个界面，将缓存的文字刷到屏幕上
            u8g2_DrawStr(&display->u8g2, 0, 20, display->current_msg);

            u8g2_SendBuffer(&display->u8g2);
            vTaskDelay(pdMS_TO_TICKS(100));  // 100ms刷新一次
        }
    }

public:
    Mini12864Display() {
        // === U8g2 硬件 SPI 初始化 ===
        u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
        u8g2_esp32_hal.clk = DISPLAY_SCLK_PIN;
        u8g2_esp32_hal.mosi = DISPLAY_MOSI_PIN;
        u8g2_esp32_hal.cs = DISPLAY_CS_PIN;
        u8g2_esp32_hal.dc = DISPLAY_DC_PIN;
        u8g2_esp32_hal.reset = DISPLAY_RST_PIN;
        u8g2_esp32_hal_init(u8g2_esp32_hal);

        // 绑定 ST7565 驱动
        u8g2_Setup_st7565_ea_dogm128_f(&u8g2, U8G2_R0, u8g2_esp32_spi_byte_cb,
                                       u8g2_esp32_gpio_and_delay_cb);

        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetContrast(&u8g2, 10);

        // 启动后台刷新任务
        xTaskCreate(display_task, "lcd_task", 4096, this, 2, NULL);
    }

    // === 重写小智系统调用的虚函数，把内容映射到单色屏上 ===

    virtual void SetChatMessage(const char* role, const char* content) override {
        // 小智在说话时系统会调用这个函数
        // 我们把内容截取并存下来，后台 task 会自动把它刷到屏幕上
        snprintf(current_msg, sizeof(current_msg), "%s: %s", role, content);
    }

    virtual bool Lock(int timeout_ms = 0) override {
        // 单色屏刷新很快，我们直接返回 true 假装上锁成功
        return true;
    }

    virtual void Unlock() override {
        // 留空即可
    }
};

// --------------------------------------------------------
// 定义开发板类，继承 WifiBoard ST7565
// --------------------------------------------------------
class MyC3Board : public WifiBoard {
private:
    Display* display_;

public:
    MyC3Board() {
        // InitializeSpi();
        // InitializeDisplay();
        display_ = new Mini12864Display();
    }

    // SPI初始化（用于显示屏）
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 显示屏初始化 ST7565
    void InitializeDisplay() {
        // 实例化物理驱动
        St7565r* hw_lcd = new St7565r(SPI2_HOST,        //
                                      DISPLAY_CS_PIN,   // CS 屏幕侧
                                      DISPLAY_DC_PIN,   // A0 / DC
                                      DISPLAY_RST_PIN,  // RST
                                      GPIO_NUM_NC       // BL
        );
        hw_lcd->Initialize();

        // 实例化 LVGL 适配层 (屏幕分辨率 128x64)
        display_ = new St7565rDisplay(hw_lcd, 128, 64);
    }

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