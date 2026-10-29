#include "SSD1306.hpp"
#include "esp_log.h"

static const char *TAG = "SSD1306";

// Standard ASCII 5x7 font subset
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // Space (0x20)
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x14, 0x08, 0x3E, 0x08, 0x14, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x08, 0x14, 0x22, 0x41, 0x00, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x00, 0x41, 0x22, 0x14, 0x08, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x61, 0x51, 0x49, 0x45, 0x43  // Z
};

SSD1306::SSD1306(i2c_port_t port, uint8_t addr, SemaphoreHandle_t mutex)
    : m_port(port), m_addr(addr), m_mutex(mutex) {
    memset(m_buffer, 0, sizeof(m_buffer));
}

void SSD1306::sendCommand(uint8_t cmd) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        i2c_cmd_handle_t h = i2c_cmd_link_create();
        i2c_master_start(h);
        i2c_master_write_byte(h, (m_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(h, 0x00, true); // Control byte: Command
        i2c_master_write_byte(h, cmd, true);
        i2c_master_stop(h);
        i2c_master_cmd_begin(m_port, h, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(h);
        xSemaphoreGive(m_mutex);
    }
}

void SSD1306::sendData(const uint8_t* data, size_t length) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        i2c_cmd_handle_t h = i2c_cmd_link_create();
        i2c_master_start(h);
        i2c_master_write_byte(h, (m_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(h, 0x40, true); // Control byte: Data
        i2c_master_write(h, const_cast<uint8_t*>(data), length, true);
        i2c_master_stop(h);
        i2c_master_cmd_begin(m_port, h, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(h);
        xSemaphoreGive(m_mutex);
    }
}

bool SSD1306::begin() {
    ESP_LOGI(TAG, "Initializing SSD1306 OLED Controller at 0x%02X", m_addr);

    sendCommand(0xAE); // Display Off
    sendCommand(0xD5); sendCommand(0x80); // Display Clock Div
    sendCommand(0xA8); sendCommand(0x3F); // Mux Ratio (64 lines)
    sendCommand(0xD3); sendCommand(0x00); // Display Offset
    sendCommand(0x40);                    // Start Line = 0
    sendCommand(0x8D); sendCommand(0x14); // Enable Charge Pump
    sendCommand(0x20); sendCommand(0x00); // Horizontal Addressing Mode
    sendCommand(0xA1);                    // Segment Re-map
    sendCommand(0xC8);                    // COM Output Scan Direction
    sendCommand(0xDA); sendCommand(0x12); // COM Pins config
    sendCommand(0x81); sendCommand(0xCF); // Contrast
    sendCommand(0xD9); sendCommand(0xF1); // Pre-charge
    sendCommand(0xDB); sendCommand(0x40); // VCOMH Deselect
    sendCommand(0xA4);                    // Entire Display On resume
    sendCommand(0xA6);                    // Normal Display (not inverted)
    sendCommand(0xAF);                    // Display On

    clear();
    display();
    return true;
}

void SSD1306::clear() {
    memset(m_buffer, 0, sizeof(m_buffer));
}

void SSD1306::display() {
    // Set Column & Page range for 128x64 GDDRAM
    sendCommand(0x21); // Set Column Address
    sendCommand(0x00); // Start = 0
    sendCommand(0x7F); // End = 127
    sendCommand(0x22); // Set Page Address
    sendCommand(0x00); // Start Page = 0
    sendCommand(0x07); // End Page = 7

    sendData(m_buffer, sizeof(m_buffer));
}

void SSD1306::drawPixel(int x, int y, bool color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    if (color) {
        m_buffer[x + (y / 8) * 128] |= (1 << (y % 8));
    } else {
        m_buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
    }
}

void SSD1306::drawChar(int x, int y, char c, bool color, int size) {
    if (c < 32 || c > 90) {
        if (c >= 'a' && c <= 'z') c -= 32; // Convert lowercase to upper
        else c = 32;
    }
    int fontIndex = (c - 32) * 5;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[fontIndex + col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                if (size == 1) {
                    drawPixel(x + col, y + row, color);
                } else {
                    for (int sx = 0; sx < size; sx++) {
                        for (int sy = 0; sy < size; sy++) {
                            drawPixel(x + col * size + sx, y + row * size + sy, color);
                        }
                    }
                }
            }
        }
    }
}

void SSD1306::drawString(int x, int y, const char* str, int size) {
    if (!str) return;
    int curX = x;
    while (*str) {
        drawChar(curX, y, *str, true, size);
        curX += (6 * size);
        str++;
    }
}

