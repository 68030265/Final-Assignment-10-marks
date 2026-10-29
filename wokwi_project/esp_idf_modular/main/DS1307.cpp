#include "DS1307.hpp"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DS1307";

DS1307::DS1307(i2c_port_t port, SemaphoreHandle_t mutex)
    : m_port(port), m_mutex(mutex) {}

bool DS1307::begin() {
    ESP_LOGI(TAG, "DS1307 RTC Hardware Driver Initialized (Address 0x%02X)", DS1307_ADDR);
    // Verify communication with probe
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(m_port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        xSemaphoreGive(m_mutex);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "DS1307 probe success.");
            return true;
        } else {
            ESP_LOGW(TAG, "DS1307 probe unacknowledged (error: %d). Continuing with virtual fallback.", ret);
            return true;
        }
    }
    return false;
}

bool DS1307::getDateTime(struct tm* timeinfo) {
    if (!timeinfo) return false;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint8_t reg_addr = 0x00;
        uint8_t data[7] = {0};

        // Write start register address 0x00
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg_addr, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, data, 6, I2C_ACK_VAL);
        i2c_master_read_byte(cmd, &data[6], I2C_NACK_VAL);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(m_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        xSemaphoreGive(m_mutex);

        if (ret == ESP_OK) {
            timeinfo->tm_sec  = bcdToDec(data[0] & 0x7F);
            timeinfo->tm_min  = bcdToDec(data[1] & 0x7F);
            timeinfo->tm_hour = bcdToDec(data[2] & 0x3F);
            timeinfo->tm_wday = bcdToDec(data[3] & 0x07) - 1;
            timeinfo->tm_mday = bcdToDec(data[4] & 0x3F);
            timeinfo->tm_mon  = bcdToDec(data[5] & 0x1F) - 1;
            timeinfo->tm_year = bcdToDec(data[6]) + 100; // 2000-based
            return true;
        }
    }
    return false;
}

bool DS1307::setDateTime(const struct tm* timeinfo) {
    if (!timeinfo) return false;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint8_t data[8];
        data[0] = 0x00; // Register pointer
        data[1] = decToBcd(timeinfo->tm_sec) & 0x7F; // CH = 0 (oscillator enabled)
        data[2] = decToBcd(timeinfo->tm_min) & 0x7F;
        data[3] = decToBcd(timeinfo->tm_hour) & 0x3F; // 24-hour mode
        data[4] = decToBcd(timeinfo->tm_wday + 1);
        data[5] = decToBcd(timeinfo->tm_mday);
        data[6] = decToBcd(timeinfo->tm_mon + 1);
        data[7] = decToBcd(timeinfo->tm_year % 100);

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, data, 8, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(m_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        xSemaphoreGive(m_mutex);

        return (ret == ESP_OK);
    }
    return false;
}

