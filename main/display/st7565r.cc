#include "st7565r.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ST7565R_HW";

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
    devcfg.clock_speed_hz = 10 * 1000 * 1000;
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
    ESP_LOGI(TAG, "Init ST7565R Hardware");
    Reset();

    SendCommand(0xE2); // Reset
    vTaskDelay(pdMS_TO_TICKS(10));

    SendCommand(0xA2); // 1/9 Bias
    SendCommand(0xA0); // ADC Normal
    SendCommand(0xC8); // COM Reverse
    SendCommand(0x40); // Start line 0

    SendCommand(0x28 | 0x07); // Power control
    vTaskDelay(pdMS_TO_TICKS(50));
    
    SendCommand(0x20 | 0x05); // Resistor ratio
    SendCommand(0x81); // Volume
    SendCommand(0x18); // Contrast
    
    SendCommand(0xA4); // All points normal
    SendCommand(0xA6); // Display normal
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
        SendCommand(0x00); // Lower column
        SendCommand(0x10); // Upper column
        SendData(&buffer_[page * 128], 128);
    }
}