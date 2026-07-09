#include "audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display.h"
#include "display/st7565r.h"
#include "display/st7565r_display.h"
#include "wifi_board.h"

// --------------------------------------------------------
// 定义开发板类，继承 WifiBoard ST7565
// --------------------------------------------------------
class MyC3Board : public WifiBoard {
private:
    Display* display_;

public:
    MyC3Board() {
        InitializeSpi();
        InitializeDisplay();
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