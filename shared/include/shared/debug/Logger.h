// logger with levels categories and file output

#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <ctime>

namespace KnC {
namespace Debug {

// log levels in priority order low to high
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

// log categories
enum class LogCategory {
    General,
    Core,
    Graphics,
    Physics,
    Network,
    Audio,
    Input,
    Game,
    Custom
};

// log message structure
struct LogMessage {
    LogLevel level;
    LogCategory category;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::chrono::system_clock::time_point timestamp;
    std::size_t threadId;
};

// log sink interface for custom output
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void Write(const LogMessage& msg) = 0;
    virtual void Flush() = 0;
};

// console log sink
class ConsoleSink : public ILogSink {
public:
    ConsoleSink(bool colored = true);
    void Write(const LogMessage& msg) override;
    void Flush() override;

private:
    bool m_colored;
    std::mutex m_mutex;
};

// file log sink
class FileSink : public ILogSink {
public:
    FileSink(const std::string& filename, bool append = false);
    ~FileSink();

    void Write(const LogMessage& msg) override;
    void Flush() override;

private:
    std::ofstream m_file;
    std::mutex m_mutex;
};

// main logger class singleton
class Logger {
public:
    static Logger& Instance();
    
    void SetMinLevel(LogLevel level) { m_minLevel = level; }
    LogLevel GetMinLevel() const { return m_minLevel; }
    
    void SetCategoryLevel(LogCategory category, LogLevel level);
    
    void AddSink(std::shared_ptr<ILogSink> sink);
    void RemoveSink(std::shared_ptr<ILogSink> sink);
    void ClearSinks();
    
    void Log(LogLevel level, LogCategory category,
             const std::string& message,
             const char* file = nullptr, 
             int line = 0,
             const char* function = nullptr);
    
    void Trace(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Trace, cat, msg, file, line, func);
    }
    
    void Debug(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Debug, cat, msg, file, line, func);
    }
    
    void Info(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Info, cat, msg, file, line, func);
    }
    
    void Warning(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Warning, cat, msg, file, line, func);
    }
    
    void Error(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Error, cat, msg, file, line, func);
    }
    
    void Fatal(LogCategory cat, const std::string& msg, const char* file = nullptr, int line = 0, const char* func = nullptr) {
        Log(LogLevel::Fatal, cat, msg, file, line, func);
    }
    
    void Flush();

    static const char* LevelToString(LogLevel level);
    static const char* CategoryToString(LogCategory category);
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    bool ShouldLog(LogLevel level, LogCategory category) const;
    std::string FormatMessage(const LogMessage& msg) const;
    
    LogLevel m_minLevel;
    std::unordered_map<LogCategory, LogLevel> m_categoryLevels;
    std::vector<std::shared_ptr<ILogSink>> m_sinks;
    std::mutex m_mutex;
};

}} // namespace KnC Debug

#define LOG_TRACE(category, message) \
    KnC::Debug::Logger::Instance().Trace(category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_DEBUG(category, message) \
    KnC::Debug::Logger::Instance().Debug(category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_INFO(category, message) \
    KnC::Debug::Logger::Instance().Info(category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_WARNING(category, message) \
    KnC::Debug::Logger::Instance().Warning(category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_ERROR(category, message) \
    KnC::Debug::Logger::Instance().Error(category, message, __FILE__, __LINE__, __FUNCTION__)

#define LOG_FATAL(category, message) \
    KnC::Debug::Logger::Instance().Fatal(category, message, __FILE__, __LINE__, __FUNCTION__)

// formatted logging printf-style
#define LOG_TRACE_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_TRACE(category, buf); }

#define LOG_DEBUG_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_DEBUG(category, buf); }

#define LOG_INFO_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_INFO(category, buf); }

#define LOG_WARNING_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_WARNING(category, buf); }

#define LOG_ERROR_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_ERROR(category, buf); }

#define LOG_FATAL_F(category, fmt, ...) \
    { char buf[1024]; snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); LOG_FATAL(category, buf); }

// conditional compilation disabled in release
#ifdef NDEBUG
    #undef LOG_TRACE
    #undef LOG_DEBUG
    #undef LOG_TRACE_F
    #undef LOG_DEBUG_F
    #define LOG_TRACE(category, message)
    #define LOG_DEBUG(category, message)
    #define LOG_TRACE_F(category, fmt, ...)
    #define LOG_DEBUG_F(category, fmt, ...)
#endif

