/**
 * @file Logger.cpp
 * @brief Thread-safe logging with file rotation and ANSI colors
 */

#include "logging/Logger.h"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace knc {

// ANSI color codes
namespace color {
    const char* reset   = "\033[0m";
    const char* bold    = "\033[1m";
    const char* red     = "\033[31m";
    const char* green   = "\033[32m";
    const char* yellow  = "\033[33m";
    const char* blue    = "\033[34m";
    const char* magenta = "\033[35m";
    const char* cyan    = "\033[36m";
    const char* white   = "\033[37m";
    const char* gray    = "\033[90m";
}

static bool g_colorsEnabled = false;

void EnableAnsiColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
#endif
    g_colorsEnabled = true;
}

const char* GetCategoryColor(const std::string& cat) {
    if (!g_colorsEnabled) return "";
    
    if (cat == "MAIN" || cat == "INIT") return color::white;
    if (cat == "LOGIN" || cat == "AUTH") return color::cyan;
    if (cat == "GAME" || cat == "WORLD") return color::green;
    if (cat == "PACKET" || cat == "NET") return color::blue;
    if (cat == "DB" || cat == "DATABASE") return color::magenta;
    if (cat == "HANDSHAKE") return color::cyan;
    if (cat == "SESSION") return color::yellow;
    if (cat == "ROOM" || cat == "LOBBY") return color::green;
    return color::gray;
}

void Logger::init(const std::string& filepath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filepath = filepath;
    m_minLevel = minLevel;
    
    EnableAnsiColors();
    
    // Create directory if needed
    auto dir = std::filesystem::path(filepath).parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }
    
    m_file.open(filepath, std::ios::app);
    if (!m_file.is_open()) {
        std::cerr << "Failed to open log file: " << filepath << std::endl;
    }
}

void Logger::log(LogLevel level, const std::string& category, const std::string& message) {
    if (level < m_minLevel) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    rotateIfNeeded();
    
    // Build plain text for file
    std::ostringstream fileLine;
    fileLine << timestamp() << " [" << levelToString(level) << "] [" << category << "] " << message;
    
    // Build colored text for console
    std::ostringstream consoleLine;
    const char* levelColor = color::gray;
    const char* catColor = GetCategoryColor(category);
    
    switch (level) {
        case LogLevel::LVL_DEBUG: levelColor = color::gray; break;
        case LogLevel::LVL_INFO:  levelColor = color::white; break;
        case LogLevel::LVL_WARN:  levelColor = color::yellow; break;
        case LogLevel::LVL_ERROR: levelColor = color::red; break;
    }
    
    if (g_colorsEnabled) {
        consoleLine << color::gray << timestamp() << " "
                   << levelColor << "[" << levelToString(level) << "]" << color::reset << " "
                   << catColor << "[" << category << "]" << color::reset << " "
                   << message;
    } else {
        consoleLine << fileLine.str();
    }
    
    // Console output
    if (level == LogLevel::LVL_ERROR) {
        std::cerr << consoleLine.str() << std::endl;
    } else {
        std::cout << consoleLine.str() << std::endl;
    }
    
    // File output (no colors)
    if (m_file.is_open()) {
        m_file << fileLine.str() << std::endl;
        m_file.flush();
    }
}

void Logger::rotateIfNeeded() {
    if (!m_file.is_open()) return;
    
    m_file.seekp(0, std::ios::end);
    if (static_cast<size_t>(m_file.tellp()) > m_maxFileSize) {
        m_file.close();
        
        // Rename old file with timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << m_filepath << "." << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        std::filesystem::rename(m_filepath, oss.str());
        
        m_file.open(m_filepath, std::ios::app);
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::LVL_DEBUG: return "DEBUG";
        case LogLevel::LVL_INFO:  return "INFO ";
        case LogLevel::LVL_WARN:  return "WARN ";
        case LogLevel::LVL_ERROR: return "ERROR";
        default: return "?????";
    }
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void PrintServerHeader(const char* serverName, const char* version) {
    EnableAnsiColors();
    
    // center the server name - YES i lose my time on this xD
    std::string name(serverName);
    int padding = (45 - (int)name.length()) / 2;
    std::string padLeft(padding > 0 ? padding : 0, ' ');
    std::string padRight(45 - padding - (int)name.length(), ' ');
    
    std::cout << color::cyan
              << "-----------------------------------------------\n"
              << "|" << padLeft << color::bold << serverName << color::reset << color::cyan << padRight << "|\n"
              << "|              Chibi Kart / KNC               |\n"
              << "|                 Version " << version << "                 |\n"
              << "-----------------------------------------------\n"
              << "|               by davedevils                 |\n"
              << "-----------------------------------------------\n"
              << color::reset << std::endl;
}

} // namespace knc
