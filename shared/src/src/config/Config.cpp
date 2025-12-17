/**
 * @file Config.cpp
 * @brief JSON configuration loader
 */

#include "config/Config.h"
#include "logging/Logger.h"
#include <fstream>

namespace knc {

bool Config::load(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("CONFIG", "Cannot open: " + filepath);
            return false;
        }
        m_data = nlohmann::json::parse(file);
        LOG_INFO("CONFIG", "Loaded: " + filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CONFIG", std::string("Parse error: ") + e.what());
        return false;
    }
}

bool Config::save(const std::string& filepath) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        file << m_data.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace knc

