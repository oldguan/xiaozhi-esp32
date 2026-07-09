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


#include <Arduino.h>
#include <U8g2lib.h>

#define LED_PIN 8
// 保持我们成功的物理接线
#define LCD_CS 10
#define LCD_DC 2    // 屏幕 A0
#define LCD_CLK 6   // 屏幕 SCLK
#define LCD_DATA 7  // 屏幕 SDA
#define LCD_RST 3   // 【重新启用 GPIO 3 作为复位脚】

// 锁定 ST7565 的 4线 软件 SPI 构造函数
U8G2_ST7565_EA_DOGM128_F_4W_SW_SPI u8g2(U8G2_R0, LCD_CLK, LCD_DATA, LCD_CS, LCD_DC, LCD_RST);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // 1. 初始化屏幕
  u8g2.begin();

  // 2. 降低通讯速率，给老芯片足够的反应时间
  u8g2.setBusClock(100000);

  // 3. 【核心物理调整】：发送 ST7565 专属的底层控制命令
  // 这些命令会直接控制芯片内部的电荷泵，强行拉高偏置电压
  u8g2.sendF("c", 0xA2);  // 设置液晶偏置比为 1/9 Bias（最清晰的标准比例）
  u8g2.sendF("c", 0x2F);  // 打开芯片内部所有的增压、随动和稳压电路（稳压拉满）
  u8g2.sendF("c", 0x24);  // 设置粗调对比度 (范围 0x20-0x27，0x24 中等偏浓)

  // 4. 【微调对比度】：精细控制文字的浓淡
  // 如果烧录后字还是很淡，把下面的 150 改成 180 甚至 220！
  // 如果烧录后屏幕全黑变一团墨水，把 150 改小到 100 或者 80！
  u8g2.setContrast(126);

  Serial.println("ST7565 深度电压初始化配置完成！");
}
void family() {
  // =========================================================================
  // 🎨 【核心：像素图形几何作画 - 手牵手一家人】
  // =========================================================================

  // 1. 【左边：爸爸 👨】
  u8g2.drawDisc(30, 20, 5);       // 爸爸的头部（实心圆）
  u8g2.drawBox(27, 26, 7, 16);    // 爸爸的身体（长方形）
  u8g2.drawLine(25, 41, 25, 56);  // 爸爸的左腿
  u8g2.drawLine(35, 41, 35, 56);  // 爸爸的右腿
  u8g2.drawLine(27, 30, 20, 42);  // 爸爸的外侧手（垂下）
  u8g2.drawLine(33, 30, 42, 38);  // 爸爸的内侧手（斜下伸出，牵孩子）

  // 2. 【右边：妈妈 👩】
  u8g2.drawDisc(98, 20, 5);     // 妈妈的头部
  u8g2.drawBox(95, 26, 7, 10);  // 妈妈的身体上半身
  u8g2.drawTriangle(91, 41, 106, 41, 98, 30);  // 妈妈的裙子下摆（三角形）
  u8g2.drawLine(94, 41, 94, 56);               // 妈妈的左腿
  u8g2.drawLine(102, 41, 102, 56);             // 妈妈的右腿
  u8g2.drawLine(101, 30, 108, 42);             // 妈妈的外侧手（垂下）
  u8g2.drawLine(95, 30, 86, 38);               // 妈妈的内侧手（斜下伸出，牵孩子）

  // 3. 【中间：小朋友 👧👦】
  u8g2.drawDisc(64, 28, 4);       // 小朋友的头部（稍微低一点、小一点）
  u8g2.drawBox(61, 33, 7, 12);    // 小朋友的身体
  u8g2.drawLine(62, 45, 62, 54);  // 小朋友的左腿
  u8g2.drawLine(66, 45, 66, 54);  // 小朋友的右腿
  u8g2.drawLine(61, 36, 42, 38);  // 小朋友的左手 -> 直奔爸爸递过来的手（手牵手连线！）
  u8g2.drawLine(67, 36, 86, 38);  // 小朋友的右手 -> 直奔妈妈递过来的手（手牵手连线！）

  // 4. 【点缀温馨地面】
  u8g2.drawLine(10, 57, 118, 57);  // 画一条地平线，让一家人稳稳站立

  // 5. 【顶部温馨文字】
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);  // 使用精美汉字库
  u8g2.drawUTF8(40, 10, "幸福一家人");
}
void success() {
  // 画一个全屏边框，用来看边缘像素是否对齐、清晰
  //u8g2.drawFrame(0, 0, 128, 64);
  u8g2.drawRFrame(5, 5, 118, 54, 4);  // 参数：X, Y, 宽, 高, 圆角半径

  // 用 C3 恐怖的算力直接刷出清爽的中文字体
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(10, 25, "可乐我们成功啦！");

  // 第二行显示芯片确认信息
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 48, "DRV: ST7565 CLEAR");
}
void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);

  u8g2.clearBuffer();
  family();
  u8g2.sendBuffer();

  digitalWrite(LED_PIN, LOW);
  delay(500);
}
