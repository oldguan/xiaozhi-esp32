#ifndef _ST7565R_H_
#define _ST7565R_H_

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <string.h>

class St7565r {
public:
    St7565r(spi_host_device_t spi_host, gpio_num_t cs, gpio_num_t dc, gpio_num_t rst, gpio_num_t bl = GPIO_NUM_NC);
    ~St7565r();

    void Initialize();
    void SetBacklight(bool on);
    
    // 写入单个像素到内部缓冲
    void DrawPixel(int x, int y, uint8_t color);
    
    // 清空内部缓冲
    void Clear(uint8_t color = 0);
    
    // 将整个 1024 字节缓冲推送到物理屏幕
    void Flush();

private:
    void SendCommand(uint8_t cmd);
    void SendData(const uint8_t *data, int len);
    void Reset();

    spi_device_handle_t spi_;
    gpio_num_t dc_pin_;
    gpio_num_t rst_pin_;
    gpio_num_t bl_pin_;

    // 内部屏幕缓冲区 (128*64/8 = 1024 bytes)
    uint8_t buffer_[1024];
};

#endif // _ST7565R_H_