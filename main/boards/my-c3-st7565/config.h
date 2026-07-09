#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_
#include <driver/gpio.h>

// 1. I2S 音频引脚 (麦克风与喇叭时钟引脚物理并联复用)
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_1 // LRCK
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_0 // BCLK
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_5 // INMP441 SD
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_2 // MAX98357 DIN
#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_NC // 没有MCLK则留空

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_8
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_9
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// ==========================================
// Mini12864 (ST7565) 屏幕 SPI 引脚配置
// ==========================================
#define DISPLAY_SCLK_PIN GPIO_NUM_4     // SCLK / CLK
#define DISPLAY_MOSI_PIN GPIO_NUM_6     // SDA / SI
#define DISPLAY_CS_PIN   GPIO_NUM_7     // CS (屏幕显示侧)
#define DISPLAY_DC_PIN   GPIO_NUM_3     // A0 / DC
#define DISPLAY_RST_PIN  GPIO_NUM_10    // RST

#define DISPLAY_WIDTH            128
#define DISPLAY_HEIGHT           64

// 3. 板载 LED 灯
#define BUILTIN_LED_GPIO         GPIO_NUM_8
#define LED_ACTIVE_LEVEL         0 // C3 Super Mini 是低电平点亮
#endif

 