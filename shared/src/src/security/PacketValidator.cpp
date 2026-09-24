/// frame checks and the before login opcode list

#include "security/PacketValidator.h"
#include "net/Protocol.h"

namespace knc {

ValidationResult PacketValidator::validate(const Packet& packet) {
    if (packet.totalSize() < PACKET_HEADER_SIZE) {
        return ValidationResult::INVALID_SIZE;
    }

    if (packet.payload().size() > kMaxC2SPayload) {
        return ValidationResult::INVALID_SIZE;
    }

    // the low byte alone dropped 0x0100 the quest discard
    if (packet.opcode() == 0x0000) {
        return ValidationResult::INVALID_CMD;
    }

    return ValidationResult::OK;
}

bool PacketValidator::allowedBeforeAuth(uint16_t opcode) {
    switch (opcode) {
        case CMD::C_HEARTBEAT:
        case CMD::C_CLIENT_AUTH:
        case CMD::C_FULL_STATE:
        case CMD::C_CLIENT_INFO:
        case CMD::S_SESSION_CONFIRM:     // C2S 0xA7 carries the redirect ticket
        case CMD::C_LOBBY_TELEMETRY:
        case 0x000B:                     // ping reply
            return true;
        default:
            return false;
    }
}

bool PacketValidator::payloadIsSecret(uint16_t opcode) {
    switch (opcode) {
        case CMD::C_CLIENT_AUTH:
        case CMD::C_LAUNCHER_LOGIN:
        case CMD::C_CLIENT_INFO:
        case CMD::S_SESSION_CONFIRM:
        case CMD::I_SERVER_REGISTER:
            return true;
        default:
            return false;
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

}
