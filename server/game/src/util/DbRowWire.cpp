/// shared row accessor and ascii clamp core see include util DbRowWire h

#include "util/DbRowWire.h"

#include <algorithm>
#include <cstdlib>

namespace knc {

std::string clampAsciiCore(const std::string& in, size_t maxChars, AsciiNulMode mode,
                            size_t& nulProcessedLen) {
    if (mode == AsciiNulMode::kIgnoreNul) {
        nulProcessedLen = in.size();
        if (in.size() <= maxChars) return in;
        return in.substr(0, maxChars);
    }

    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (c == '\0') {
            if (mode == AsciiNulMode::kStopAtNul) break;
            continue;  // kStripNul drops the byte and keeps scanning
        }
        out.push_back(c);
    }
    nulProcessedLen = out.size();
    if (out.size() > maxChars) out.resize(maxChars);
    return out;
}

std::string rowStrCore(const DbRow& row, const char* key) {
    const auto it = row.find(key);
    return it == row.end() ? std::string() : it->second;
}

int64_t rowInt64Throwing(const DbRow& row, const char* key, int64_t fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    try {
        return static_cast<int64_t>(std::stoll(it->second));
    } catch (...) {
        return fallback;
    }
}

uint64_t rowUInt64Throwing(const DbRow& row, const char* key, uint64_t fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    try {
        return static_cast<uint64_t>(std::stoull(it->second));
    } catch (...) {
        return fallback;
    }
}

float rowFloatThrowing(const DbRow& row, const char* key, float fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    try {
        return std::stof(it->second);
    } catch (...) {
        return fallback;
    }
}

int64_t rowInt64NoThrow(const DbRow& row, const char* key, int64_t fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    return std::strtoll(it->second.c_str(), nullptr, 10);
}

uint64_t rowUInt64NoThrow(const DbRow& row, const char* key, uint64_t fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    return std::strtoull(it->second.c_str(), nullptr, 10);
}

float rowFloatNoThrow(const DbRow& row, const char* key, float fallback) {
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return fallback;
    return std::strtof(it->second.c_str(), nullptr);
}

}  // namespace knc
