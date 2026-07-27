### ESP32C3 接入 https://github.com/78/xiaozhi-esp32 项目
- mini12864 屏幕
    - 针脚 CS CLK SI SO SDA SCLK A0 RST CS NC +5v GND , CS CLK SI SO 这四个标记为ROM_SPI
    - 驱动芯片 ST7565
- ESP32 c3 型号
    - ESP32-C3 Super Mini


---------------------------------------------------

ESP32-C3 Super Mini 
          +------------+
          | [ Type-C ] |
    GND --|  [  5V  ]  |-- 5V -------- (接 12864 屏幕 5V / VCC)
    GND --|  [ GND  ]  |-- GND ------- (接 12864 屏幕 GUND / GND)
   GPIO3 -|  [  3   ]  |-- GPIO2 ----- 对应 12864 屏幕 A0 (DC)
          |  [  ..  ]  |
   GPIO6 -|  [  6   ]  |-- GPIO10 ---- 对应 12864 屏幕 CS (显示侧)
   GPIO7 -|  [  7   ]  |
          +------------+
            |      |
            |      +-------> 对应 12864 屏幕 SDA / SI
            +--------------> 对应 12864 屏幕 SCLK / CLK
            
   (注：GPIO3 此时用杜邦线横跨接过去，连接到 12864 ST7565 屏幕的 RST 针脚)

12864 LCD ST7565 屏幕端（控制侧标签）	💥 对应的 ESP32-C3 Super Mini 物理引脚	信号类型与软件定义
5V / VCC	                    5V	屏幕及背光的主供电正极
GUND / GND	                    GND	屏幕及背光的主供电负极
屏幕显示侧的 CS	                GPIO 10	屏幕显示芯片的片选脚（#define LCD_CS 10）
屏幕显示侧的 A0 (或DC/RS)	    GPIO 2	数据/命令控制选择脚（#define LCD_DC 2）
屏幕显示侧的 SCLK (或CLK)	    GPIO 6	串行通讯时钟脚（#define LCD_CLK 6）
屏幕显示侧的 SDA (或SI/MOSI)	GPIO 7	串行通讯数据脚（#define LCD_DATA 7）
屏幕显示侧的 RST	            GPIO 3 开机复位脚（#define LCD_RST 3）


 一、 MAX98357 (喇叭功放) 完整接线表MAX98357 引脚连接到哪里关键说明LRCESP32 的 Pin 1左右声道时钟 (WS)，与麦克风物理并联BCLKESP32 的 Pin 0位时钟，与麦克风物理并联DINESP32 的 Pin 2ESP32 发给功放的音频数据GAIN悬空 (NC)什么都不接，默认 9dB 增益，最稳定SD5V 或 3.3V⚠️ 必须接！功放的开机键，悬空会彻底没声音GNDGND电源地线VIN5V功放电源 (接 5V 喇叭推力才够大)🎙️ 二、 INMP441 (麦克风) 完整接线表INMP441 引脚连接到哪里关键说明WSESP32 的 Pin 1左右声道时钟，与功放物理并联SCKESP32 的 Pin 0位时钟 (BCLK)，与功放物理并联SDESP32 的 Pin 5麦克风发给 ESP32 的音频数据L/RGND⚠️ 必须接！接地代表让麦克风输出左声道VDD3.3V⚠️ 必须接 3.3V！(千万别接 5V，芯片会秒烧)GNDGND电源地线