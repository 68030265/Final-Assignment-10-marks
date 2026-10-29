#include "AlarmManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ALARM";

AlarmManager::AlarmManager(gpio_num_t buzzerPin)
    : m_buzzerPin(buzzerPin), m_alarmHour(7), m_alarmMin(30), m_enabled(true), m_isRinging(false) {}

void AlarmManager::init() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << m_buzzerPin);
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(m_buzzerPin, 0);

    ESP_LOGI(TAG, "Alarm Buzzer Initialized on GPIO %d", m_buzzerPin);
}

void AlarmManager::setAlarm(int hour, int min, bool enable) {
    m_alarmHour = hour;
    m_alarmMin  = min;
    m_enabled   = enable;
    ESP_LOGI(TAG, "Alarm set to %02d:%02d [%s]", hour, min, enable ? "ARMED" : "OFF");
}

bool AlarmManager::checkAlarm(int currHour, int currMin, int currSec) {
    if (m_enabled && !m_isRinging) {
        if (currHour == m_alarmHour && currMin == m_alarmMin && currSec < 2) {
            m_isRinging = true;
            ESP_LOGW(TAG, "ALARM TRIGGERED! Current time matches %02d:%02d", m_alarmHour, m_alarmMin);
            // Toggle buzzer pin
            gpio_set_level(m_buzzerPin, 1);
            return true;
        }
    }
    return false;
}

void AlarmManager::stopRinging() {
    if (m_isRinging) {
        m_isRinging = false;
        gpio_set_level(m_buzzerPin, 0);
        ESP_LOGI(TAG, "Alarm silenced by user.");
    }
}

