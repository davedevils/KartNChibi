
#include <gtest/gtest.h>
#include "logging/Logger.h"
#include <filesystem>

using namespace knc;

TEST(Logger, Init) {
    Logger::instance().init("logs/test.log", LogLevel::LVL_DEBUG);
    EXPECT_TRUE(std::filesystem::exists("logs"));
}

TEST(Logger, LogLevels) {
    // must not crash
    LOG_DEBUG("TEST", "Debug message");
    LOG_INFO("TEST", "Info message");
    LOG_WARN("TEST", "Warning message");
    LOG_ERROR("TEST", "Error message");
}

