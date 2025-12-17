/**
 * @file Logger.h
 * @brief Thread-safe logging with file rotation
 */

#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>

namespace knc {

enum class LogLevel { LVL_DEBUG, LVL_INFO, LVL_WARN, LVL_ERROR };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(const std::string& filepath, LogLevel minLevel = LogLevel::LVL_INFO);
    void log(LogLevel level, const std::string& category, const std::string& message);
    
    void debug(const std::string& cat, const std::string& msg) { log(LogLevel::LVL_DEBUG, cat, msg); }
    void info(const std::string& cat, const std::string& msg)  { log(LogLevel::LVL_INFO, cat, msg); }
    void warn(const std::string& cat, const std::string& msg)  { log(LogLevel::LVL_WARN, cat, msg); }
    void error(const std::string& cat, const std::string& msg) { log(LogLevel::LVL_ERROR, cat, msg); }

private:
    Logger() = default;
    std::mutex m_mutex;
    std::ofstream m_file;
    LogLevel m_minLevel = LogLevel::LVL_INFO;
    size_t m_maxFileSize = 10 * 1024 * 1024; // 10MB
    std::string m_filepath;
    
    void rotateIfNeeded();
    std::string levelToString(LogLevel level);
    std::string timestamp();
};

// Convenience macros
#define LOG_DEBUG(cat, msg) knc::Logger::instance().debug(cat, msg)
#define LOG_INFO(cat, msg)  knc::Logger::instance().info(cat, msg)
#define LOG_WARN(cat, msg)  knc::Logger::instance().warn(cat, msg)
#define LOG_ERROR(cat, msg) knc::Logger::instance().error(cat, msg)

// Print a styled server header
void PrintServerHeader(const char* serverName, const char* version = "1.0");

} // namespace knc

