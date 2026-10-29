#include "Stopwatch.hpp"

Stopwatch::Stopwatch()
    : m_running(false), m_startTimeMs(0), m_accumulatedMs(0) {}

void Stopwatch::start(uint32_t currentMs) {
    if (!m_running) {
        m_startTimeMs = currentMs;
        m_running = true;
    }
}

void Stopwatch::pause(uint32_t currentMs) {
    if (m_running) {
        m_accumulatedMs += (currentMs - m_startTimeMs);
        m_running = false;
    }
}

void Stopwatch::reset() {
    m_running = false;
    m_accumulatedMs = 0;
    m_startTimeMs = 0;
    m_laps.clear();
}

uint32_t Stopwatch::recordLap(uint32_t currentMs) {
    uint32_t el = getElapsedMs(currentMs);
    if (m_laps.size() < MAX_LAPS) {
        m_laps.push_back(el);
    }
    return el;
}

uint32_t Stopwatch::getElapsedMs(uint32_t currentMs) const {
    if (m_running) {
        return m_accumulatedMs + (currentMs - m_startTimeMs);
    }
    return m_accumulatedMs;
}

uint32_t Stopwatch::getLap(size_t index) const {
    if (index < m_laps.size()) {
        return m_laps[index];
    }
    return 0;
}

