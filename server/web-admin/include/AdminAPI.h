/**
 * @file AdminAPI.h
 * @brief REST API helper functions
 */

#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace knc {

// Helper functions for API responses
namespace AdminAPI {
    
    // Create JSON error response
    inline nlohmann::json error(const std::string& message, int code = 400) {
        return {{"error", message}, {"code", code}};
    }
    
    // Create JSON success response
    inline nlohmann::json success(const std::string& message = "OK") {
        return {{"success", true}, {"message", message}};
    }
    
    // Escape SQL string (basic - use prepared statements in production!)
    inline std::string escapeSql(const std::string& input) {
        std::string output;
        output.reserve(input.size() * 2);
        for (char c : input) {
            if (c == '\'' || c == '"' || c == '\\' || c == '\0') {
                output += '\\';
            }
            output += c;
        }
        return output;
    }
    
} // namespace AdminAPI

} // namespace knc
