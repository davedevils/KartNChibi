/// the chat money and exp commands are a local test aid so a public server keeps them off

#pragma once
#include <cstdint>

namespace knc {

/// on for an operator account or when KNC DEV COMMANDS is exactly 1 off for everyone else
inline bool devCommandsEnabled(const char* envValue, uint8_t gmLevel) {
    if (gmLevel > 0) return true;
    return envValue != nullptr && envValue[0] == '1' && envValue[1] == '\0';
}

}
