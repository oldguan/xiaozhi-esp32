#include "st7565r.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ST7565_HW";

St7565r::St7565r(spi_host_device_t spi_host, gpio_num_t cs, gpio_num_t dc, gpio_num_t rst, gpio_num_t bl)
    : dc_pin_(dc), rst_pin_(rst), bl_pin_(bl) {
    
    memset(buffer_, 0, sizeof(buffer_));

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << dc_pin_) | (1ULL << rst_pin_);
    if (bl_pin_ != GPIO_NUM_NC) {
        io_conf.pin_bit_mask |= (1ULL << bl_pin_);
    }
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 4 * 1000 * 1000; // 稳定降频到 4MHz
    devcfg.mode = 0;
    devcfg.spics_io_num = cs;
    devcfg.queue_size = 7;
    
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &devcfg, &spi_));
}

St7565r::~St7565r() {
    spi_bus_remove_device(spi_);
}

void St7565r::SendCommand(uint8_t cmd) {
    gpio_set_level(dc_pin_, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(spi_, &t);
}

void St7565r::SendData(const uint8_t *data, int len) {
    if (len == 0) return;
    gpio_set_level(dc_pin_, 1);
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(spi_, &t);
}

void St7565r::Reset() {
    gpio_set_level(rst_pin_, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst_pin_, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void St7565r::Initialize() {
    ESP_LOGI(TAG, "Init ST7565 Hardware");
    Reset();

    SendCommand(0xE2); // Soft Reset
    vTaskDelay(pdMS_TO_TICKS(50));

    SendCommand(0xA3); // 1/7 Bias (增强驱动电压)
    SendCommand(0xA0); // ADC Normal (如果左右反了改为 0xA1)
    SendCommand(0xC8); // COM Reverse (如果上下反了改为 0xC0)
    SendCommand(0xA6); // Normal Display
    SendCommand(0x40); // Start line 0

    // 分步开启电源 (防花屏)
    SendCommand(0x2C); 
    vTaskDelay(pdMS_TO_TICKS(5));
    SendCommand(0x2E); 
    vTaskDelay(pdMS_TO_TICKS(5));
    SendCommand(0x2F); 
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // 提升对比度
    SendCommand(0x20 | 0x06); // 粗调
    SendCommand(0x81);        
    SendCommand(0x30);        // 微调 (如果太黑，调小为 0x28；如果太淡，调大为 0x35)
    
    SendCommand(0xA4); // All points normal

    // 开机先清空随机内存
    Clear(0); 
    Flush();  

    SendCommand(0xAF); // Display ON
    SetBacklight(true);
}

void St7565r::SetBacklight(bool on) {
    if (bl_pin_ != GPIO_NUM_NC) {
        gpio_set_level(bl_pin_, on ? 1 : 0);
    }
}

void St7565r::DrawPixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    
    int page = y / 8;
    int bit = y % 8;
    int index = page * 128 + x;
    
    if (color) {
        buffer_[index] |= (1 << bit);
    } else {
        buffer_[index] &= ~(1 << bit);
    }
}

void St7565r::Clear(uint8_t color) {
    memset(buffer_, color ? 0xFF : 0x00, sizeof(buffer_));
}

void St7565r::Flush() {
    for (int page = 0; page < 8; page++) {
        SendCommand(0xB0 | page);
        SendCommand(0x00); // 列地址低 4 位 (如果画面最左侧有雪花边，改成 0x04)
        SendCommand(0x10); // 列地址高 4 位
        SendData(&buffer_[page * 128], 128);
    }
}