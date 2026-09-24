/// C2S frames no stock client sends whose old handlers paid gold or wrote state

#pragma once
#include <cstddef>
#include <cstdint>

namespace knc {

/// true when the frame is dropped before the switch the opcode pages list no client sender for these shapes
inline bool isRetiredC2S(uint16_t opcode, size_t payloadSize) {
    switch (opcode) {
        case 0x001B: case 0x001C: case 0x001D:   // S2C owned lists only
        case 0x0030:                             // S2C room state only 0x0130 is the option 11 float
        case 0x0036:                             // S2C nullsub the laps come from 0x41 checkpoints
        case 0x00A9: case 0x00AB: case 0x00AC:   // old tutorial and licence test routes
        case 0x00C7: case 0x00C8: case 0x00C9: case 0x00CA:   // old scenario menu chapter stage start
            return true;
        case 0x00AA:
            return payloadSize != 4;             // only the 4 byte GhostEnter is real
        case 0x00CB:
            return payloadSize < 20;             // ConsumableUseNotify is 29 bytes
        default:
            return false;
    }
}

}
