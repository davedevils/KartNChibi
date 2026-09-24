/// rate limiter tests for the anti cheat component

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "security/RateLimiter.h"

using namespace knc;

TEST(RateLimiter, AllowsFirstPacket) {
    RateLimiter limiter;
    EXPECT_TRUE(limiter.check(0x01));
}

TEST(RateLimiter, AllowsMultipleDifferentCmds) {
    RateLimiter limiter;
    EXPECT_TRUE(limiter.check(0x01));
    EXPECT_TRUE(limiter.check(0x02));
    EXPECT_TRUE(limiter.check(0x03));
}

TEST(RateLimiter, InitialDropCountZero) {
    RateLimiter limiter;
    EXPECT_EQ(limiter.getDroppedCount(), 0);
}

TEST(RateLimiter, BlocksRapidSameCmd) {
    RateLimiter::Config config;
    config.defaultMinIntervalMs = 100;
    RateLimiter limiter(config);
    
    EXPECT_TRUE(limiter.check(0x01));
    
    EXPECT_FALSE(limiter.check(0x01));
    
    EXPECT_EQ(limiter.getDroppedCount(), 1);
}

TEST(RateLimiter, AllowsAfterInterval) {
    RateLimiter::Config config;
    config.defaultMinIntervalMs = 50;
    RateLimiter limiter(config);
    
    EXPECT_TRUE(limiter.check(0x01));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    EXPECT_TRUE(limiter.check(0x01));
}

TEST(RateLimiter, CustomOpcodeInterval) {
    RateLimiter::Config config;
    config.defaultMinIntervalMs = 100;
    config.opcodeMinIntervalMs[0xA6] = 10;  // heartbeat 0xA6 gets a faster interval
    RateLimiter limiter(config);
    
    EXPECT_TRUE(limiter.check(0xA6));
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_TRUE(limiter.check(0xA6));  // 15ms sleep clears the 10ms interval
}

TEST(RateLimiter, GlobalRateLimit) {
    RateLimiter::Config config;
    config.globalMaxPerSec = 5;
    config.defaultMinIntervalMs = 0;
    RateLimiter limiter(config);
    
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.check(static_cast<uint8_t>(i)));
    }
    
    EXPECT_FALSE(limiter.check(0x06));
}

TEST(RateLimiter, ResetClearsState) {
    RateLimiter::Config config;
    config.defaultMinIntervalMs = 100;
    RateLimiter limiter(config);
    
    limiter.check(0x01);
    limiter.check(0x01);  // blocked
    EXPECT_EQ(limiter.getDroppedCount(), 1);
    
    limiter.reset();
    
    EXPECT_EQ(limiter.getDroppedCount(), 0);
    EXPECT_TRUE(limiter.check(0x01));
}

TEST(RateLimiter, SpeedHackDetection) {
    // Speed hackers often send position updates too fast
    RateLimiter::Config config;
    config.opcodeMinIntervalMs[0x31] = 30;  // CMD 0x31 is player position
    RateLimiter limiter(config);
    
    int allowed = 0;
    int blocked = 0;
    
    for (int i = 0; i < 20; ++i) {
        if (limiter.check(0x31)) {
            ++allowed;
        } else {
            ++blocked;
        }
    }
    
    // only first allowed no sleep between iterations
    EXPECT_EQ(allowed, 1);
    EXPECT_EQ(blocked, 19);
}

TEST(RateLimiter, NormalPlayAllowed) {
    RateLimiter::Config config;
    config.globalMaxPerSec = 100;
    config.defaultMinIntervalMs = 10;
    RateLimiter limiter(config);
    
    // normal player varied commands with delay
    int allowed = 0;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (limiter.check(static_cast<uint8_t>(i % 5))) {
            ++allowed;
        }
    }
    
    EXPECT_GE(allowed, 8);
}

