#ifndef NTPMANAGER_HPP
#define NTPMANAGER_HPP

#include <time.h>
#include <stdbool.h>

class NTPManager {
public:
    NTPManager(const char* ssid, const char* pass);
    void init();
    bool syncTime();
    bool isConnected() const { return m_connected; }
    bool isSynced() const { return m_synced; }

private:
    const char* m_ssid;
    const char* m_pass;
    bool        m_connected;
    bool        m_synced;
};

#endif // NTPMANAGER_HPP

