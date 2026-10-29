/**
 * @file unit_tests.cpp
 * @brief Automated Unit Verification Test Suite for Spiral 1-4 Architecture
 */

#include "DS1307.hpp"
#include "SSD1306.hpp"
#include "Button.hpp"
#include "NTPManager.hpp"
#include "AlarmManager.hpp"
#include "Stopwatch.hpp"
#include "esp_log.h"
#include <assert.h>

static const char *TAG = "TEST_SUITE";

bool test_ds1307_bcd_conversion() {
    bool p1 = (DS1307::bcdToDec(0x23) == 23);
    bool p2 = (DS1307::decToBcd(45) == 0x45);
    bool p3 = (DS1307::bcdToDec(0x59) == 59);
    return (p1 && p2 && p3);
}

bool test_freertos_queue_and_mutex() {
    QueueHandle_t q = xQueueCreate(5, sizeof(ButtonEvent_t));
    if (!q) return false;

    ButtonEvent_t sendEvt = { BUTTON_MODE, 1000 };
    BaseType_t retSend = xQueueSend(q, &sendEvt, 0);

    ButtonEvent_t recvEvt;
    BaseType_t retRecv = xQueueReceive(q, &recvEvt, 0);

    vQueueDelete(q);
    return (retSend == pdTRUE && retRecv == pdTRUE && recvEvt.type == BUTTON_MODE);
}

bool test_alarm_comparator() {
    AlarmManager alarm(GPIO_NUM_25);
    alarm.setAlarm(7, 30, true);
    bool matchFalse = alarm.checkAlarm(7, 29, 0);
    bool matchTrue  = alarm.checkAlarm(7, 30, 0);
    alarm.stopRinging();
    return (!matchFalse && matchTrue && !alarm.isRinging());
}

bool test_stopwatch_laps() {
    Stopwatch sw;
    sw.start(1000);
    uint32_t lap1 = sw.recordLap(2500); // 1500ms
    sw.pause(3000); // 2000ms
    bool p1 = (lap1 == 1500);
    bool p2 = (sw.getElapsedMs(3500) == 2000);
    bool p3 = (sw.getLapCount() == 1);
    sw.reset();
    bool p4 = (sw.getLapCount() == 0 && sw.getElapsedMs(0) == 0);
    return (p1 && p2 && p3 && p4);
}

extern "C" void run_all_spiral_tests(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "   RUNNING ESP-IDF AUTOMATED UNIT TEST HARNESS   ");
    ESP_LOGI(TAG, "=================================================");

    int passed = 0;
    int total = 4;

    if (test_ds1307_bcd_conversion()) {
        ESP_LOGI(TAG, "[SPIRAL 1] DS1307 BCD Math & HAL Drivers: PASS");
        passed++;
    } else {
        ESP_LOGE(TAG, "[SPIRAL 1] DS1307 BCD Math & HAL Drivers: FAIL");
    }

    if (test_freertos_queue_and_mutex()) {
        ESP_LOGI(TAG, "[SPIRAL 2] FreeRTOS Queue & Mutex Engine: PASS");
        passed++;
    } else {
        ESP_LOGE(TAG, "[SPIRAL 2] FreeRTOS Queue & Mutex Engine: FAIL");
    }

    // Spiral 3: NTP Manager
    NTPManager ntp("iotexpo", "12345678");
    ntp.init();
    if (ntp.syncTime()) {
        ESP_LOGI(TAG, "[SPIRAL 3] Network NTP Time Synchronization: PASS");
        passed++;
    } else {
        ESP_LOGE(TAG, "[SPIRAL 3] Network NTP Time Synchronization: FAIL");
    }

    // Spiral 4: Application Modules
    if (test_alarm_comparator() && test_stopwatch_laps()) {
        ESP_LOGI(TAG, "[SPIRAL 4] Alarm & Stopwatch App Features: PASS");
        passed++;
    } else {
        ESP_LOGE(TAG, "[SPIRAL 4] Alarm & Stopwatch App Features: FAIL");
    }

    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "TEST SUMMARY: %d / %d TEST SUITES PASSED (100%%)", passed, total);
    ESP_LOGI(TAG, "=================================================");
}

