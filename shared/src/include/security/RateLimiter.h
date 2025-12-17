/**
 * @file RateLimiter.h
 * @brief Per-connection and per-opcode rate limiting
 */

#pragma once
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace knc {

class RateLimiter {
public:
    struct Config {
        int globalMaxPerSec = 100;
        int defaultMinIntervalMs = 50;
        std::unordered_map<uint8_t, int> opcodeMinIntervalMs;
    };
    
    RateLimiter() = default;
    explicit RateLimiter(const Config& config) : m_config(config) {}
    
    // Returns true if packet should be allowed, false if rate limited
    bool check(uint8_t cmd);
    
    // Reset all counters (e.g., on new connection)
    void reset();
    
    // Stats
    int getDroppedCount() const { return m_droppedCount; }

private:
    Config m_config;
    std::unordered_map<uint8_t, std::chrono::steady_clock::time_point> m_lastPacketTime;
    int m_packetsThisSecond = 0;
    std::chrono::steady_clock::time_point m_secondStart;
    int m_droppedCount = 0;
};

} // namespace knc

