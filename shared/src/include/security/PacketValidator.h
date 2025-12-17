/**
 * @file PacketValidator.h
 * @brief Packet validation and state checking
 */

#pragma once
#include "net/Packet.h"
#include <string>

namespace knc {

enum class ValidationResult {
    OK,
    INVALID_SIZE,
    INVALID_CMD,
    INVALID_STATE,
    INVALID_DATA
};

class PacketValidator {
public:
    // Validate packet structure
    static ValidationResult validate(const Packet& packet);
    
    // Check if command is valid for current state
    static bool isValidForState(uint8_t cmd, int currentState);
    
    // Get human-readable error
    static std::string resultToString(ValidationResult result);
};

} // namespace knc

