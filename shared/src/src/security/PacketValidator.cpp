/**
 * @file PacketValidator.cpp
 * @brief Packet validation and state checking
 */

#include "security/PacketValidator.h"
#include "net/Protocol.h"

namespace knc {

ValidationResult PacketValidator::validate(const Packet& packet) {
    // Check size bounds
    if (packet.totalSize() < PACKET_HEADER_SIZE) {
        return ValidationResult::INVALID_SIZE;
    }
    
    if (packet.totalSize() > PACKET_MAX_SIZE) {
        return ValidationResult::INVALID_SIZE;
    }
    
    // CMD 0x00 is invalid
    if (packet.cmd() == 0x00) {
        return ValidationResult::INVALID_CMD;
    }
    
    return ValidationResult::OK;
}

bool PacketValidator::isValidForState(uint8_t cmd, int currentState) {
    // States: 0=disconnected, 1=connecting, 2=connected, 3=authenticating, 
    //         4=authenticated, 5=menu, 6=garage, 7=shop, 8=lobby, 9=room, 
    //         10=loading, 11=racing, 12=results
    
    switch (cmd) {
        // Always allowed
        case CMD::C_HEARTBEAT:
        case CMD::C_DISCONNECT:
            return true;
        
        // Only during connection/auth
        case CMD::C_FULL_STATE:
        case CMD::C_CLIENT_AUTH:
            return currentState <= 4;
        
        // Only when authenticated
        case CMD::C_CHAT_MESSAGE:
        case CMD::C_WHISPER:
            return currentState >= 4;
        
        // Shop packets only in shop
        case CMD::C_SHOP_ENTER:
        case CMD::C_SHOP_EXIT:
            return currentState == 7 || currentState >= 4;
        
        default:
            return currentState >= 4;  // Default: must be authenticated
    }
}

std::string PacketValidator::resultToString(ValidationResult result) {
    switch (result) {
        case ValidationResult::OK: return "OK";
        case ValidationResult::INVALID_SIZE: return "Invalid packet size";
        case ValidationResult::INVALID_CMD: return "Invalid command";
        case ValidationResult::INVALID_STATE: return "Invalid for current state";
        case ValidationResult::INVALID_DATA: return "Invalid data";
        default: return "Unknown error";
    }
}

} // namespace knc
