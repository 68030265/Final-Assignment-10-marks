#include "Button.hpp"
#include "esp_log.h"

static const char *TAG = "BUTTON";

ButtonDriver::ButtonDriver(gpio_num_t pin, ButtonType_t type, QueueHandle_t queue)
    : m_pin(pin), m_type(type), m_queue(queue), m_lastState(1), m_lastDebounceTime(0) {}

void ButtonDriver::init() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << m_pin);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Button Configured on GPIO %d (Type: %d)", m_pin, m_type);
}

void ButtonDriver::scan(uint32_t currentMillis) {
    int readVal = gpio_get_level(m_pin);

    if (readVal != m_lastState) {
        if ((currentMillis - m_lastDebounceTime) > DEBOUNCE_MS) {
            m_lastDebounceTime = currentMillis;
            m_lastState = readVal;

            if (readVal == 0) { // Active Low (Pushed)
                ButtonEvent_t evt;
                evt.type = m_type;
                evt.timestamp = currentMillis;
                if (m_queue != NULL) {
                    xQueueSend(m_queue, &evt, 0);
                    ESP_LOGD(TAG, "Button %d queued", m_type);
                }
            }
        }
    }
}

