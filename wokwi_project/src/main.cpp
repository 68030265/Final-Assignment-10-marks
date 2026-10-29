/**
 * @file main.cpp
 * @brief ESP32 FreeRTOS Modular Digital Clock Firmware (VS Code / PlatformIO / Wokwi)
 * @details Fully integrated DS1307 RTC, SSD1306 OLED, 4-Button FreeRTOS Queue,
 *          Wi-Fi NTP Auto-Sync, Piezo Buzzer Alarm, and Precision Stopwatch with Laps.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ====================================================================
// HARDWARE PIN & ADDRESS DEFINITIONS
// ====================================================================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define OLED_I2C_ADDR       0x3C

#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

#define PIN_BTN_MODE        12
#define PIN_BTN_INC         13
#define PIN_BTN_SET         14
#define PIN_BTN_LAP         27
#define PIN_BUZZER          25

// Wi-Fi Credentials for Wokwi Simulation & Standard AP
const char* WIFI_SSIDS[]    = {"Wokwi-GUEST", "iotexpo"};
const char* WIFI_PASSWORDS[] = {"", "12345678"};
const int   NUM_NETWORKS    = 2;

// NTP Server Configuration (Bangkok / ICT UTC+7)
const char* NTP_SERVER      = "pool.ntp.org";
const long  GMT_OFFSET_SEC  = 7 * 3600;       // UTC + 7
const int   DAYLIGHT_OFFSET = 0;              // No DST in Thailand

// ====================================================================
// FREERTOS ENUMS & STRUCTURES
// ====================================================================
enum ButtonId_t {
    BTN_NONE = 0,
    BTN_MODE,
    BTN_INC,
    BTN_SET,
    BTN_LAP
};

enum ClockMode_t {
    MODE_MAIN_CLOCK = 0,
    MODE_ALARM_SET,
    MODE_STOPWATCH,
    MODE_MANUAL_TIME,
    MODE_COUNT
};

const char* MODE_NAMES[] = {
    "MAIN CLOCK",
    "ALARM SET",
    "STOPWATCH",
    "MANUAL TIME"
};

struct ButtonEvent_t {
    ButtonId_t id;
    uint32_t   pressTimeMs;
};

// ====================================================================
// GLOBAL OBJECTS & STATE
// ====================================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS1307       rtc;

// FreeRTOS Handles
QueueHandle_t     xButtonQueue = NULL;
SemaphoreHandle_t xI2CMutex    = NULL;

// System State
volatile ClockMode_t currentMode = MODE_MAIN_CLOCK;
bool wifiConnected = false;
bool ntpSynced     = false;

// Alarm State
int  alarmHour     = 7;
int  alarmMinute   = 30;
bool alarmArmed    = true;
bool alarmRinging  = false;
unsigned long alarmTriggerTime = 0;

// Stopwatch State
bool     swRunning     = false;
uint32_t swElapsedMs   = 0;
uint32_t swStartTick   = 0;
#define MAX_LAPS 5
uint32_t swLaps[MAX_LAPS];
int      swLapCount    = 0;

// Manual Time Adjustment Buffer
int manualHour = 12;
int manualMin  = 0;
int manualSec  = 0;

// RTC Backup Time
DateTime currentDateTime(2026, 10, 29, 11, 0, 0);

// ====================================================================
// BUZZER SOUND ROUTINES
// ====================================================================
void buzzerTone(int freq, int durationMs) {
    tone(PIN_BUZZER, freq, durationMs);
    delay(durationMs);
    noTone(PIN_BUZZER);
}

void buzzerBeepShort() {
    tone(PIN_BUZZER, 2400, 50);
}

void buzzerAlarmPattern() {
    tone(PIN_BUZZER, 2500, 150);
    delay(180);
    tone(PIN_BUZZER, 3000, 150);
    delay(180);
}

// ====================================================================
// BUTTON SCANNER FREERTOS TASK (Core 0, Priority 2)
// Implements 50ms software debouncing and event queueing
// ====================================================================
void taskButtonScanner(void* pvParameters) {
    pinMode(PIN_BTN_MODE, INPUT_PULLUP);
    pinMode(PIN_BTN_INC,  INPUT_PULLUP);
    pinMode(PIN_BTN_SET,  INPUT_PULLUP);
    pinMode(PIN_BTN_LAP,  INPUT_PULLUP);

    const int debounceDelay = 50; // ms
    int lastStateMode = HIGH, lastStateInc = HIGH, lastStateSet = HIGH, lastStateLap = HIGH;
    unsigned long lastDebounceMode = 0, lastDebounceInc = 0, lastDebounceSet = 0, lastDebounceLap = 0;

    Serial.println("[BUTTON] Switch Scanner Task running on Core 0");

    for (;;) {
        unsigned long currentMillis = millis();

        // 1. Scan MODE button
        int readMode = digitalRead(PIN_BTN_MODE);
        if (readMode != lastStateMode && (currentMillis - lastDebounceMode > debounceDelay)) {
            lastDebounceMode = currentMillis;
            lastStateMode = readMode;
            if (readMode == LOW) {
                ButtonEvent_t evt = { BTN_MODE, currentMillis };
                xQueueSend(xButtonQueue, &evt, 0);
            }
        }

        // 2. Scan INC button
        int readInc = digitalRead(PIN_BTN_INC);
        if (readInc != lastStateInc && (currentMillis - lastDebounceInc > debounceDelay)) {
            lastDebounceInc = currentMillis;
            lastStateInc = readInc;
            if (readInc == LOW) {
                ButtonEvent_t evt = { BTN_INC, currentMillis };
                xQueueSend(xButtonQueue, &evt, 0);
            }
        }

        // 3. Scan SET button
        int readSet = digitalRead(PIN_BTN_SET);
        if (readSet != lastStateSet && (currentMillis - lastDebounceSet > debounceDelay)) {
            lastDebounceSet = currentMillis;
            lastStateSet = readSet;
            if (readSet == LOW) {
                ButtonEvent_t evt = { BTN_SET, currentMillis };
                xQueueSend(xButtonQueue, &evt, 0);
            }
        }

        // 4. Scan LAP button
        int readLap = digitalRead(PIN_BTN_LAP);
        if (readLap != lastStateLap && (currentMillis - lastDebounceLap > debounceDelay)) {
            lastDebounceLap = currentMillis;
            lastStateLap = readLap;
            if (readLap == LOW) {
                ButtonEvent_t evt = { BTN_LAP, currentMillis };
                xQueueSend(xButtonQueue, &evt, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// ====================================================================
// NTP SYNC TASK (Core 0, Priority 1)
// Connects to Wi-Fi and synchronizes internal clock & DS1307
// ====================================================================
void taskNtpSync(void* pvParameters) {
    Serial.println("[WIFI] Initializing Station Mode connection...");

    for (int i = 0; i < NUM_NETWORKS; i++) {
        Serial.printf("[WIFI] Connecting to SSID: '%s'...\n", WIFI_SSIDS[i]);
        WiFi.begin(WIFI_SSIDS[i], WIFI_PASSWORDS[i]);
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 15) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
            retry++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            Serial.printf("\n[WIFI] Connected! Assigned IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        }
        Serial.println("\n[WIFI] Connect failed. Trying next SSID...");
    }

    if (wifiConnected) {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER, "time.google.com");
        Serial.println("[NTP] Requesting SNTP timestamp from pool.ntp.org...");
        
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            ntpSynced = true;
            Serial.printf("[NTP] Time Synchronized: %02d:%02d:%02d %04d-%02d-%02d (ICT UTC+7)\n",
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
                xSemaphoreGive(xI2CMutex);
                Serial.println("[DS1307] Hardware RTC Calibrated against NTP Master Time.");
            }
        } else {
            Serial.println("[NTP] Failed to obtain time from NTP server.");
        }
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1800000)); // Every 30 mins
        if (WiFi.status() == WL_CONNECTED) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 3000)) {
                if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
                    xSemaphoreGive(xI2CMutex);
                }
            }
        }
    }
}

// ====================================================================
// RENDER HELPERS FOR SSD1306 OLED (128x64)
// ====================================================================
void renderHeaderBar() {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.printf("[%s]", MODE_NAMES[currentMode]);
    display.setCursor(80, 0);
    display.print(wifiConnected ? "WIFI:OK" : "WIFI:--");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
}

void renderMainClockUI(const DateTime& now) {
    renderHeaderBar();

    display.setTextSize(2);
    display.setCursor(16, 18);
    display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

    display.setTextSize(1);
    display.setCursor(22, 40);
    const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    display.printf("%04d-%02d-%02d (%s)", now.year(), now.month(), now.day(), daysOfWeek[now.dayOfTheWeek()]);

    display.setCursor(18, 54);
    if (alarmArmed) {
        display.printf("ALM ON %02d:%02d", alarmHour, alarmMinute);
    } else {
        display.print("ALARM: OFF");
    }
}

void renderAlarmSetUI() {
    renderHeaderBar();

    display.setTextSize(1);
    display.setCursor(15, 14);
    display.print("CONFIG ALARM TIME");

    display.setTextSize(2);
    display.setCursor(20, 26);
    display.printf("%02d:%02d", alarmHour, alarmMinute);

    display.setTextSize(1);
    display.setCursor(84, 30);
    display.print(alarmArmed ? "[ARMED]" : "[OFF]");

    display.setCursor(0, 48);
    display.print("INC:+5m | SET:ARM/OFF");
    display.setCursor(0, 56);
    display.print("LAP:Reset | MODE:Exit");
}

void renderStopwatchUI() {
    renderHeaderBar();

    uint32_t currentElapsed = swElapsedMs;
    if (swRunning) {
        currentElapsed += (millis() - swStartTick);
    }

    uint32_t mins = currentElapsed / 60000;
    uint32_t secs = (currentElapsed % 60000) / 1000;
    uint32_t cs   = (currentElapsed % 1000) / 10;

    display.setTextSize(2);
    display.setCursor(18, 16);
    display.printf("%02d:%02d.%02d", mins, secs, cs);

    display.setTextSize(1);
    display.setCursor(10, 36);
    display.printf("Status: %s", swRunning ? "RUNNING" : "PAUSED");

    display.setCursor(10, 46);
    display.printf("Laps: %d / %d", swLapCount, MAX_LAPS);

    if (swLapCount > 0) {
        uint32_t lastLap = swLaps[swLapCount - 1];
        display.setCursor(10, 56);
        display.printf("Last: %02d:%02d.%02d", lastLap / 60000, (lastLap % 60000) / 1000, (lastLap % 1000) / 10);
    } else {
        display.setCursor(10, 56);
        display.print("SET:Start | LAP:Lap");
    }
}

void renderManualTimeUI() {
    renderHeaderBar();

    display.setTextSize(1);
    display.setCursor(10, 14);
    display.print("MANUAL TIME CALIBRATION");

    display.setTextSize(2);
    display.setCursor(20, 28);
    display.printf("%02d:%02d:%02d", manualHour, manualMin, manualSec);

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("INC:+Min | SET:+Hour");
    display.setCursor(0, 56);
    display.print("LAP:Save to RTC");
}

void renderAlarmRingingUI() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 10);
    display.print("! ALARM !");

    display.setTextSize(1);
    display.setCursor(12, 35);
    display.printf("TIME MATCH: %02d:%02d", alarmHour, alarmMinute);

    display.setCursor(6, 52);
    display.print("PRESS ANY BTN TO STOP");
    display.display();
}

// ====================================================================
// EVENT DISPATCHER & STATE MACHINE
// ====================================================================
void handleButtonEvent(ButtonId_t id) {
    buzzerBeepShort();

    if (alarmRinging) {
        alarmRinging = false;
        noTone(PIN_BUZZER);
        Serial.println("[ALARM] Alarm silenced by user button press.");
        return;
    }

    switch (id) {
        case BTN_MODE: {
            currentMode = (ClockMode_t)((currentMode + 1) % MODE_COUNT);
            Serial.printf("[FSM] Mode Switched -> %s\n", MODE_NAMES[currentMode]);
            if (currentMode == MODE_MANUAL_TIME) {
                manualHour = currentDateTime.hour();
                manualMin  = currentDateTime.minute();
                manualSec  = currentDateTime.second();
            }
            break;
        }

        case BTN_INC: {
            if (currentMode == MODE_MAIN_CLOCK) {
                Serial.println("[ACTION] Manual Force NTP Sync requested...");
                if (WiFi.status() == WL_CONNECTED) {
                    struct tm ti;
                    if (getLocalTime(&ti, 3000)) {
                        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            rtc.adjust(DateTime(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                                                ti.tm_hour, ti.tm_min, ti.tm_sec));
                            xSemaphoreGive(xI2CMutex);
                            Serial.println("[ACTION] NTP Sync Successful!");
                        }
                    }
                } else {
                    Serial.println("[ACTION] Wi-Fi not connected, cannot sync NTP.");
                }
            } else if (currentMode == MODE_ALARM_SET) {
                alarmMinute = (alarmMinute + 5) % 60;
                if (alarmMinute == 0) {
                    alarmHour = (alarmHour + 1) % 24;
                }
                Serial.printf("[ALARM] Set Alarm Time: %02d:%02d\n", alarmHour, alarmMinute);
            } else if (currentMode == MODE_MANUAL_TIME) {
                manualMin = (manualMin + 1) % 60;
                Serial.printf("[TIME] Adjusted Minute: %02d\n", manualMin);
            }
            break;
        }

        case BTN_SET: {
            if (currentMode == MODE_ALARM_SET) {
                alarmArmed = !alarmArmed;
                Serial.printf("[ALARM] Alarm %s\n", alarmArmed ? "ARMED" : "DISABLED");
            } else if (currentMode == MODE_STOPWATCH) {
                if (swRunning) {
                    swElapsedMs += (millis() - swStartTick);
                    swRunning = false;
                    Serial.printf("[STOPWATCH] Paused. Total: %u ms\n", swElapsedMs);
                } else {
                    swStartTick = millis();
                    swRunning = true;
                    Serial.println("[STOPWATCH] Started / Resumed.");
                }
            } else if (currentMode == MODE_MANUAL_TIME) {
                manualHour = (manualHour + 1) % 24;
                Serial.printf("[TIME] Adjusted Hour: %02d\n", manualHour);
            }
            break;
        }

        case BTN_LAP: {
            if (currentMode == MODE_STOPWATCH) {
                if (swRunning) {
                    if (swLapCount < MAX_LAPS) {
                        uint32_t lapTime = swElapsedMs + (millis() - swStartTick);
                        swLaps[swLapCount++] = lapTime;
                        Serial.printf("[STOPWATCH] Recorded Lap %d: %u ms\n", swLapCount, lapTime);
                    }
                } else {
                    swElapsedMs = 0;
                    swLapCount  = 0;
                    Serial.println("[STOPWATCH] Reset to 00:00.00");
                }
            } else if (currentMode == MODE_MANUAL_TIME) {
                if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    rtc.adjust(DateTime(currentDateTime.year(), currentDateTime.month(), currentDateTime.day(),
                                        manualHour, manualMin, manualSec));
                    xSemaphoreGive(xI2CMutex);
                    Serial.printf("[RTC] Manual Time Committed: %02d:%02d:%02d\n", manualHour, manualMin, manualSec);
                }
                currentMode = MODE_MAIN_CLOCK;
            }
            break;
        }

        default:
            break;
    }
}

// ====================================================================
// SERIAL CLI
// ====================================================================
void processSerialCLI() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "help") {
            Serial.println("\n--- ESP32 CLOCK SERIAL COMMANDS ---");
            Serial.println("  mode        : Cycle system mode");
            Serial.println("  inc         : Simulate INC button");
            Serial.println("  set         : Simulate SET button");
            Serial.println("  lap         : Simulate LAP button");
            Serial.println("  time        : Print current time");
            Serial.println("  alarm_test  : Test Piezo Buzzer");
            Serial.println("  sync        : Force NTP Sync");
            Serial.println("-----------------------------------\n");
        } else if (cmd == "mode") {
            handleButtonEvent(BTN_MODE);
        } else if (cmd == "inc") {
            handleButtonEvent(BTN_INC);
        } else if (cmd == "set") {
            handleButtonEvent(BTN_SET);
        } else if (cmd == "lap") {
            handleButtonEvent(BTN_LAP);
        } else if (cmd == "alarm_test") {
            buzzerAlarmPattern();
        } else if (cmd == "sync") {
            handleButtonEvent(BTN_INC);
        } else if (cmd == "time") {
            Serial.printf("[TIME] Current: %02d:%02d:%02d  Date: %04d-%02d-%02d\n",
                          currentDateTime.hour(), currentDateTime.minute(), currentDateTime.second(),
                          currentDateTime.year(), currentDateTime.month(), currentDateTime.day());
        }
    }
}

// ====================================================================
// SETUP
// ====================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================================");
    Serial.println("   ESP32 FreeRTOS Modular Digital Clock Starting...");
    Serial.println("   ESP-IDF / Arduino Core Simulation Platform");
    Serial.println("========================================================");

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    xI2CMutex    = xSemaphoreCreateMutex();
    xButtonQueue = xQueueCreate(10, sizeof(ButtonEvent_t));

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[ERROR] SSD1306 OLED allocation failed! Checking connection...");
    } else {
        Serial.println("[SSD1306] OLED Display Initialized (128x64 Monochrome).");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(14, 20);
        display.print("ESP32 DIGITAL CLOCK");
        display.setCursor(22, 34);
        display.print("BOOTING SYSTEM...");
        display.display();
    }

    if (!rtc.begin()) {
        Serial.println("[WARN] DS1307 RTC not found. Falling back to internal clock.");
    } else {
        Serial.println("[DS1307] Hardware RTC Online at I2C Address 0x68.");
        if (!rtc.isrunning()) {
            Serial.println("[DS1307] RTC lost power, setting compile time baseline...");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }

    xTaskCreatePinnedToCore(taskButtonScanner, "BtnScanner", 2048, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(taskNtpSync,       "NtpSync",    4096, NULL, 1, NULL, 0);

    buzzerBeepShort();
    Serial.println("[MAIN] System Initialization Complete. Starting Display Loop.");
}

// ====================================================================
// LOOP
// ====================================================================
void loop() {
    processSerialCLI();

    ButtonEvent_t evt;
    while (xQueueReceive(xButtonQueue, &evt, 0) == pdTRUE) {
        handleButtonEvent(evt.id);
    }

    static unsigned long lastRtcRead = 0;
    if (millis() - lastRtcRead > 200) {
        lastRtcRead = millis();
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (rtc.isrunning()) {
                currentDateTime = rtc.now();
            }
            xSemaphoreGive(xI2CMutex);
        }
    }

    if (alarmArmed && !alarmRinging) {
        if (currentDateTime.hour() == alarmHour &&
            currentDateTime.minute() == alarmMinute &&
            currentDateTime.second() < 2) {
            alarmRinging = true;
            alarmTriggerTime = millis();
            Serial.printf("[ALARM] TRIGGERED! Current time matches %02d:%02d\n", alarmHour, alarmMinute);
        }
    }

    if (alarmRinging) {
        renderAlarmRingingUI();
        buzzerAlarmPattern();

        if (millis() - alarmTriggerTime > 60000) {
            alarmRinging = false;
            noTone(PIN_BUZZER);
            Serial.println("[ALARM] Alarm timed out after 60 seconds.");
        }
        return;
    }

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        display.clearDisplay();

        switch (currentMode) {
            case MODE_MAIN_CLOCK:
                renderMainClockUI(currentDateTime);
                break;
            case MODE_ALARM_SET:
                renderAlarmSetUI();
                break;
            case MODE_STOPWATCH:
                renderStopwatchUI();
                break;
            case MODE_MANUAL_TIME:
                renderManualTimeUI();
                break;
            default:
                break;
        }

        display.display();
        xSemaphoreGive(xI2CMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(80));
}

