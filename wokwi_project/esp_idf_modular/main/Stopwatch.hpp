#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP

#include <stdint.h>
#include <vector>

class Stopwatch {
public:
    Stopwatch();
    void start(uint32_t currentMs);
    void pause(uint32_t currentMs);
    void reset();
    uint32_t recordLap(uint32_t currentMs);
    uint32_t getElapsedMs(uint32_t currentMs) const;
    bool isRunning() const { return m_running; }
    size_t getLapCount() const { return m_laps.size(); }
    uint32_t getLap(size_t index) const;

private:
    bool                  m_running;
    uint32_t              m_startTimeMs;
    uint32_t              m_accumulatedMs;
    std::vector<uint32_t> m_laps;
    static constexpr size_t MAX_LAPS = 5;
};

#endif // STOPWATCH_HPP

