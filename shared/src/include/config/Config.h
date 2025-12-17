/**
 * @file Config.h
 * @brief JSON configuration loader with defaults
 */

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <sstream>

namespace knc {

class Config {
public:
    static Config& instance() {
        static Config inst;
        return inst;
    }

    bool load(const std::string& filepath);
    bool save(const std::string& filepath);
    
    template<typename T>
    T get(const std::string& path, const T& defaultValue) const {
        try {
            auto keys = splitPath(path);
            const nlohmann::json* node = &m_data;
            for (const auto& key : keys) {
                if (!node->contains(key)) return defaultValue;
                node = &(*node)[key];
            }
            return node->get<T>();
        } catch (...) {
            return defaultValue;
        }
    }
    
    template<typename T>
    void set(const std::string& path, const T& value) {
        auto keys = splitPath(path);
        nlohmann::json* node = &m_data;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!node->contains(keys[i])) {
                (*node)[keys[i]] = nlohmann::json::object();
            }
            node = &(*node)[keys[i]];
        }
        (*node)[keys.back()] = value;
    }

    const nlohmann::json& raw() const { return m_data; }

private:
    Config() = default;
    nlohmann::json m_data;
    
    std::vector<std::string> splitPath(const std::string& path) const {
        std::vector<std::string> result;
        std::stringstream ss(path);
        std::string item;
        while (std::getline(ss, item, '.')) {
            result.push_back(item);
        }
        return result;
    }
};

} // namespace knc

