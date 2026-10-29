#ifndef DS1307_HPP
#define DS1307_HPP

#include <time.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"

class DS1307 {
public:
    DS1307(i2c_port_t port, SemaphoreHandle_t mutex);
    bool begin();
    bool getDateTime(struct tm* timeinfo);
    bool setDateTime(const struct tm* timeinfo);

    static inline uint8_t bcdToDec(uint8_t val) {
        return ((val / 16 * 10) + (val % 16));
    }
    static inline uint8_t decToBcd(uint8_t val) {
        return ((val / 10 * 16) + (val % 10));
    }

private:
    i2c_port_t        m_port;
    SemaphoreHandle_t m_mutex;
    static constexpr uint8_t DS1307_ADDR = 0x68;
};

#endif // DS1307_HPP

