/// REST API helper functions

#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace knc {

namespace AdminAPI {

    inline nlohmann::json error(const std::string& message, int code = 400) {
        return {{"error", message}, {"code", code}};
    }

    inline nlohmann::json success(const std::string& message = "OK") {
        return {{"success", true}, {"message", message}};
    }
    
    // escapes a SQL string use prepared statements in production instead
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
