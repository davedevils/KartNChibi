/// per connection and per opcode rate limiting

#pragma once
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace knc {

class RateLimiter {
public:
    // keyed on the full u16 opcode or 0x0121 and 0x21 would share one interval
    struct Config {
        int globalMaxPerSec = 100;
        int defaultMinIntervalMs = 50;
        std::unordered_map<uint16_t, int> opcodeMinIntervalMs;
    };

    RateLimiter() = default;
    explicit RateLimiter(const Config& config) : m_config(config) {}

    // true if the packet should be allowed false if rate limited
    bool check(uint16_t opcode);

    // reset all counters for example on a new connection
    void reset();

    int getDroppedCount() const { return m_droppedCount; }

private:
    Config m_config;
    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> m_lastPacketTime;
    int m_packetsThisSecond = 0;
    std::chrono::steady_clock::time_point m_secondStart;
    int m_droppedCount = 0;
};

}
