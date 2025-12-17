/**
 * @file IniConfig.h
 * @brief Simple INI file parser
 */

#pragma once
#include <string>
#include <map>
#include <fstream>
#include <sstream>

namespace knc {

class IniConfig {
public:
    bool load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        
        std::string line, currentSection;
        while (std::getline(file, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            line = line.substr(start);
            
            // Skip comments
            if (line[0] == ';' || line[0] == '#') continue;
            
            // Section header
            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != std::string::npos) {
                    currentSection = line.substr(1, end - 1);
                }
                continue;
            }
            
            // Key=Value
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string value = line.substr(eq + 1);
                
                // Trim key and value
                while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
                while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value = value.substr(1);
                while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
                
                std::string fullKey = currentSection.empty() ? key : currentSection + "." + key;
                m_data[fullKey] = value;
            }
        }
        return true;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = m_data.find(key);
        return (it != m_data.end()) ? it->second : defaultValue;
    }
    
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultValue;
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    
    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultValue;
        std::string val = it->second;
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }

private:
    std::map<std::string, std::string> m_data;
};

} // namespace knc

