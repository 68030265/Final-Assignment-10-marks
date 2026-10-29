#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

enum ButtonType_t {
    BUTTON_MODE = 0,
    BUTTON_INC,
    BUTTON_SET,
    BUTTON_LAP
};

struct ButtonEvent_t {
    ButtonType_t type;
    uint32_t     timestamp;
};

class ButtonDriver {
public:
    ButtonDriver(gpio_num_t pin, ButtonType_t type, QueueHandle_t queue);
    void init();
    void scan(uint32_t currentMillis);

private:
    gpio_num_t    m_pin;
    ButtonType_t  m_type;
    QueueHandle_t m_queue;
    int           m_lastState;
    uint32_t      m_lastDebounceTime;
    static constexpr uint32_t DEBOUNCE_MS = 50;
};

#endif // BUTTON_HPP

