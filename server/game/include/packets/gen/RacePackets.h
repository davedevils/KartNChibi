/// race core group builders for the leave race trigger and the two race start sends

#pragma once
#include "net/Packet.h"
#include <array>
#include <cstdint>

namespace knc {

/// race core wire builders see docs packets systems race core md 0x3B leave race is empty and reuses PacketBuilder entityRemove
struct RacePackets {
    /// c2s 0x3B empty body fired by the client leave or exit race action sub 4810C0
    static constexpr uint8_t OP_C_LEAVE_RACE = 0x3B;

    /// sub 47EA10 s2c 0x11F arms the race hud clock DAT 01adf3e8 encoding is unresolved so this is never sent
    static Packet raceTimerArm(int32_t value);

    /// sub 478e80 s2c 0x131 fixed 400 byte item drop rng stream one send per human racer at the grid
    static Packet itemRollStream(const std::array<uint32_t, 100>& rolls);
};

} // namespace knc
