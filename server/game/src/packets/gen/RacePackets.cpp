#include "packets/gen/RacePackets.h"

namespace knc {

namespace {
// opcodes above 0xFF need fromCmdFull else the byte ctor truncates
constexpr uint16_t OP_S_RACE_TIMER_ARM   = 0x011F;
constexpr uint16_t OP_S_ITEM_ROLL_STREAM = 0x0131;
} // namespace

Packet RacePackets::raceTimerArm(int32_t value) {
    // sub 47EA10 cmd 0x11F reads one raw i32 arm or target value
    Packet pkt = Packet::fromCmdFull(OP_S_RACE_TIMER_ARM);
    pkt.writeInt32(value);
    return pkt;
}

Packet RacePackets::itemRollStream(const std::array<uint32_t, 100>& rolls) {
    // sub 478E80 cmd 0x131 reads a hardcoded 100 times u32 with no count prefix
    Packet pkt = Packet::fromCmdFull(OP_S_ITEM_ROLL_STREAM);
    for (uint32_t v : rolls) pkt.writeUInt32(v);
    return pkt;
}

} // namespace knc
