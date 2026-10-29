#ifndef ALARMMANAGER_HPP
#define ALARMMANAGER_HPP

#include "driver/gpio.h"
#include <stdint.h>

class AlarmManager {
public:
    AlarmManager(gpio_num_t buzzerPin);
    void init();
    void setAlarm(int hour, int min, bool enable);
    bool checkAlarm(int currHour, int currMin, int currSec);
    void stopRinging();
    bool isRinging() const { return m_isRinging; }
    bool isArmed() const { return m_enabled; }
    int getHour() const { return m_alarmHour; }
    int getMinute() const { return m_alarmMin; }

private:
    gpio_num_t m_buzzerPin;
    int        m_alarmHour;
    int        m_alarmMin;
    bool       m_enabled;
    bool       m_isRinging;
};

#endif // ALARMMANAGER_HPP

