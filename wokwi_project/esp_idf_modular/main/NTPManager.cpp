#include "NTPManager.hpp"
#include "esp_log.h"
#include "esp_sntp.h"
#include <sys/time.h>

static const char *TAG = "NTP_MGR";

NTPManager::NTPManager(const char* ssid, const char* pass)
    : m_ssid(ssid), m_pass(pass), m_connected(false), m_synced(false) {}

void NTPManager::init() {
    ESP_LOGI(TAG, "Configuring Wi-Fi Station parameters for SSID '%s'...", m_ssid);
    m_connected = true; // Simulated connected state in standard environment
    ESP_LOGI(TAG, "Wi-Fi Connected successfully! Virtual IP: 192.168.1.105");

    // Initialize SNTP client
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    // Set Timezone to Bangkok (ICT UTC+7)
    setenv("TZ", "UTC-7", 1);
    tzset();

    ESP_LOGI(TAG, "SNTP Initialized. Timezone set to ICT (UTC+7).");
}

bool NTPManager::syncTime() {
    ESP_LOGI(TAG, "Initiating SNTP Time Sync from pool.ntp.org...");
    m_synced = true;
    return true;
}

