/// one core for the row accessor and ascii clamp helpers copied across builders and handlers

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace knc {

/// a database row read back as a string per column the shape every query returns
using DbRow = std::map<std::string, std::string>;

/// how clampAsciiCore treats a NUL byte found inside the source string
enum class AsciiNulMode {
    kStopAtNul,  ///< kStopAtNul ascii ends at first NUL same as C string copy kStripNul drops every NUL keep scanning rest
    kStripNul,
    kIgnoreNul   ///< do not look for a NUL at all just cut the byte count
};

/// cuts to maxChars bytes per mode nulProcessedLen is length after NUL handling before the maxChars cap
std::string clampAsciiCore(const std::string& in, size_t maxChars, AsciiNulMode mode,
                            size_t& nulProcessedLen);

/// the string field or empty when the key is missing every rowStr and toStr shares this
std::string rowStrCore(const DbRow& row, const char* key);

/// signed field parsed with stoll inside a try catch empty missing or bad text all fall back
int64_t rowInt64Throwing(const DbRow& row, const char* key, int64_t fallback);

/// unsigned field parsed with stoull inside a try catch empty missing or bad text all fall back
uint64_t rowUInt64Throwing(const DbRow& row, const char* key, uint64_t fallback);

/// float field parsed with stof inside a try catch empty missing or bad text all fall back
float rowFloatThrowing(const DbRow& row, const char* key, float fallback);

/// signed field parsed with strtoll never throws missing or empty falls back bad text reads as strtoll itself would
int64_t rowInt64NoThrow(const DbRow& row, const char* key, int64_t fallback);

/// unsigned field parsed with strtoul never throws same empty and bad text rules as above
uint64_t rowUInt64NoThrow(const DbRow& row, const char* key, uint64_t fallback);

/// float field parsed with strtof never throws same empty and bad text rules as above
float rowFloatNoThrow(const DbRow& row, const char* key, float fallback);

}  // namespace knc
