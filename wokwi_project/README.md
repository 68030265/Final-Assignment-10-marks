# Wokwi ESP32 Modular Digital Clock Project

## Overview
This project is an advanced **ESP32 Modular Digital Clock** running on FreeRTOS with a thread-safe Object-Oriented Architecture (OOP). It integrates:
- **DS1307 Hardware Real-Time Clock** (I2C Address `0x68`)
- **SSD1306 128x64 Monochrome OLED Display** (I2C Address `0x3C`)
- **4 Pushbuttons** with hardware interrupt / software debounce (`MODE`, `INC`, `SET`, `LAP`)
- **Piezo Buzzer** with PWM acoustic alerts on GPIO 25
- **Wi-Fi Station & SNTP Time Synchronization** (UTC+7 Bangkok Timezone)
- **FreeRTOS Dual-Core IPC**: Event Queue (`xButtonQueue`) and Bus Mutex (`xI2CMutex`)

---

## Pinout Connections (from `diagram.json`)

| ESP32 Pin | Connected Component | Function / Pin on Component | Wire Color |
|---|---|---|---|
| **GPIO 21** | SSD1306 & DS1307 | I2C Data (`SDA`) | Cyan |
| **GPIO 22** | SSD1306 & DS1307 | I2C Clock (`SCL`) | Yellow |
| **3V3** | SSD1306 & DS1307 | Power Supply (`VCC` / `5V`) | Red |
| **GND** | All Peripherals | Common Ground (`GND`) | Black |
| **GPIO 12** | Pushbutton `btn_mode` | Mode Select Switch (`1.1`) | Purple |
| **GPIO 13** | Pushbutton `btn_inc` | Increment / Manual NTP Sync (`1.1`) | Green |
| **GPIO 14** | Pushbutton `btn_set` | Set Alarm / Stopwatch Start-Pause (`1.1`) | Yellow |
| **GPIO 27** | Pushbutton `btn_lap` | Lap Split / Alarm Silence / Save (`1.1`) | Purple |
| **GPIO 25** | Piezo Buzzer `buzzer` | Audio PWM Output (`pin 1`) | Red |

---

## How to Run in Wokwi (1-Click Web Simulation)

1. Open your browser and navigate to: **[https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32)**
2. In the **`diagram.json`** tab, replace the contents with the contents of [`diagram.json`](diagram.json).
3. In the **`sketch.ino`** tab, paste the code from [`sketch.ino`](sketch.ino).
4. In the **`libraries.txt`** tab (create one if not present), add:
   ```text
   Adafruit SSD1306
   Adafruit GFX Library
   RTClib
   ```
5. Click **"Play"** (the green button).
6. The simulator will compile and execute immediately in the browser. You will see:
   - OLED booting up with the digital clock and date
   - Wi-Fi connecting to `Wokwi-GUEST` and auto-syncing NTP time
   - Pushbuttons interactive with click and keyboard shortcuts

---

## How to Run in VS Code Wokwi Extension

1. Ensure the **Wokwi Simulator** extension is installed in VS Code.
2. Open this folder in VS Code.
3. If using ESP-IDF:
   - Build using `idf.py build`
   - Press `F1` and run `Wokwi: Start Simulator`
4. The simulation starts directly inside VS Code using `wokwi.toml` and `diagram.json`.

---

## Operating Modes & Button Guide

| Button | GPIO | Clock Mode | Alarm Set Mode | Stopwatch Mode | Manual Time Mode |
|---|---|---|---|---|---|
| **MODE** | 12 | Cycle to Alarm Set | Cycle to Stopwatch | Cycle to Manual Time | Return to Clock |
| **INC** | 13 | Force NTP Sync | +5 Minutes (rolls hr) | -- | +1 Minute |
| **SET** | 14 | -- | Toggle Armed [ON/OFF]| Start / Pause | +1 Hour |
| **LAP** | 27 | Silence Alarm | Silence Alarm | Record Lap / Reset | Save & Commit to RTC |

---

## Serial Console Commands

You can also send ASCII commands in the Wokwi Serial Monitor:
- `help` : Lists all available CLI commands
- `mode` : Cycles to the next mode
- `inc` : Simulates pressing INC
- `set` : Simulates pressing SET
- `lap` : Simulates pressing LAP
- `time` : Prints current RTC and system time
- `alarm_test` : Triggers a 3-tone acoustic test on the piezo buzzer
- `sync` : Triggers an immediate SNTP synchronization request

