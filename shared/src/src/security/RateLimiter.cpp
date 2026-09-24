#include "security/RateLimiter.h"

namespace knc {

bool RateLimiter::check(uint16_t cmd) {
    auto now = std::chrono::steady_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_secondStart);
    if (elapsed.count() >= 1) {
        m_packetsThisSecond = 0;
        m_secondStart = now;
    }

    if (m_packetsThisSecond >= m_config.globalMaxPerSec) {
        ++m_droppedCount;
        return false;
    }

    int minInterval = m_config.defaultMinIntervalMs;
    auto it = m_config.opcodeMinIntervalMs.find(cmd);
    if (it != m_config.opcodeMinIntervalMs.end()) {
        minInterval = it->second;
    }
    
    auto lastIt = m_lastPacketTime.find(cmd);
    if (lastIt != m_lastPacketTime.end()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastIt->second).count();
        if (ms < minInterval) {
            ++m_droppedCount;
            return false;
        }
    }
    
    ++m_packetsThisSecond;
    m_lastPacketTime[cmd] = now;
    
    return true;
}

void RateLimiter::reset() {
    m_lastPacketTime.clear();
    m_packetsThisSecond = 0;
    m_secondStart = std::chrono::steady_clock::now();
    m_droppedCount = 0;
}

}
