/// frame checks run before dispatch and the list of frames allowed before login

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
    static constexpr size_t kMaxC2SPayload = PACKET_MAX_C2S_PAYLOAD;

    static ValidationResult validate(const Packet& packet);

    /// true for the few frames a socket may send before 0x07 or 0xA7 binds an account
    static bool allowedBeforeAuth(uint16_t opcode);

    /// true when the payload holds a password a token or the inter server key so no log may dump it
    static bool payloadIsSecret(uint16_t opcode);

    static std::string resultToString(ValidationResult result);
};

}
