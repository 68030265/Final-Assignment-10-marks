#ifndef SSD1306_HPP
#define SSD1306_HPP

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"

class SSD1306 {
public:
    SSD1306(i2c_port_t port, uint8_t addr, SemaphoreHandle_t mutex);
    bool begin();
    void clear();
    void display();
    void drawPixel(int x, int y, bool color);
    void drawChar(int x, int y, char c, bool color, int size = 1);
    void drawString(int x, int y, const char* str, int size = 1);

private:
    void sendCommand(uint8_t cmd);
    void sendData(const uint8_t* data, size_t length);

    i2c_port_t        m_port;
    uint8_t           m_addr;
    SemaphoreHandle_t m_mutex;
    uint8_t           m_buffer[128 * 64 / 8];
};

#endif // SSD1306_HPP

