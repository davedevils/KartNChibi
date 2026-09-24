// Logger implementation console and file sinks

#include "shared/debug/Logger.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace KnC {
namespace Debug {

ConsoleSink::ConsoleSink(bool colored) : m_colored(colored) {
}

void ConsoleSink::Write(const LogMessage& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    const char* color = "";
    const char* reset = "\033[0m";

    if (m_colored) {
        switch (msg.level) {
            case LogLevel::Trace:   color = "\033[90m"; break;
            case LogLevel::Debug:   color = "\033[36m"; break;
            case LogLevel::Info:    color = "\033[32m"; break;
            case LogLevel::Warning: color = "\033[33m"; break;
            case LogLevel::Error:   color = "\033[31m"; break;
            case LogLevel::Fatal:   color = "\033[35m"; break;
        }
    } else {
        reset = "";
    }

    std::cout << color
              << "[" << std::put_time(&tm, "%H:%M:%S") << "] "
              << "[" << Logger::LevelToString(msg.level) << "] "
              << "[" << Logger::CategoryToString(msg.category) << "] "
              << msg.message;
    
    if (msg.level >= LogLevel::Warning && !msg.file.empty()) {
        const char* filename = strrchr(msg.file.c_str(), '/');
        if (!filename) filename = strrchr(msg.file.c_str(), '\\');
        if (!filename) filename = msg.file.c_str();
        else filename++;
        
        std::cout << " (" << filename << ":" << msg.line << ")";
    }
    
    std::cout << reset << std::endl;
}

void ConsoleSink::Flush() {
    std::cout.flush();
}

FileSink::FileSink(const std::string& filename, bool append) {
    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    m_file.open(filename, mode);
    
    if (!m_file.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }
}

FileSink::~FileSink() {
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

void FileSink::Write(const LogMessage& msg) {
    if (!m_file.is_open()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);

    auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    m_file << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
           << "[" << Logger::LevelToString(msg.level) << "] "
           << "[" << Logger::CategoryToString(msg.category) << "] "
           << "[Thread:" << msg.threadId << "] "
           << msg.message;
    
    if (!msg.file.empty()) {
        m_file << " (" << msg.file << ":" << msg.line;
        if (!msg.function.empty()) {
            m_file << " in " << msg.function;
        }
        m_file << ")";
    }
    
    m_file << std::endl;
}

void FileSink::Flush() {
    if (m_file.is_open()) {
        m_file.flush();
    }
}

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_minLevel(LogLevel::Debug) {
    AddSink(std::make_shared<ConsoleSink>(true));
}

Logger::~Logger() {
    Flush();
}

void Logger::SetCategoryLevel(LogCategory category, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_categoryLevels[category] = level;
}

void Logger::AddSink(std::shared_ptr<ILogSink> sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.push_back(sink);
}

void Logger::RemoveSink(std::shared_ptr<ILogSink> sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.erase(
        std::remove(m_sinks.begin(), m_sinks.end(), sink),
        m_sinks.end()
    );
}

void Logger::ClearSinks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.clear();
}

void Logger::Log(LogLevel level, LogCategory category,
                 const std::string& message,
                 const char* file, int line, const char* function)
{
    if (!ShouldLog(level, category)) {
        return;
    }

    LogMessage msg;
    msg.level = level;
    msg.category = category;
    msg.message = message;
    msg.file = file ? file : "";
    msg.line = line;
    msg.function = function ? function : "";
    msg.timestamp = std::chrono::system_clock::now();
    msg.threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& sink : m_sinks) {
            sink->Write(msg);
        }
    }
    
    if (level == LogLevel::Fatal) {
        Flush();
        std::abort();
    }
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& sink : m_sinks) {
        sink->Flush();
    }
}

bool Logger::ShouldLog(LogLevel level, LogCategory category) const {
    if (level < m_minLevel) {
        return false;
    }

    auto it = m_categoryLevels.find(category);
    if (it != m_categoryLevels.end()) {
        if (level < it->second) {
            return false;
        }
    }
    
    return true;
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "?????";
    }
}

const char* Logger::CategoryToString(LogCategory category) {
    switch (category) {
        case LogCategory::General:  return "General";
        case LogCategory::Core:     return "Core   ";
        case LogCategory::Graphics: return "Graphics";
        case LogCategory::Physics:  return "Physics";
        case LogCategory::Network:  return "Network";
        case LogCategory::Audio:    return "Audio  ";
        case LogCategory::Input:    return "Input  ";
        case LogCategory::Game:     return "Game   ";
        case LogCategory::Custom:   return "Custom ";
        default:                    return "???????";
    }
}

}} // namespace KnC Debug

