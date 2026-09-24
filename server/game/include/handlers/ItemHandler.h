/// the two ability reports of the race wire 0xF2 class and 0xCD fire the item model lives in RaceHandler

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class Room;

/// stateless the 0x47 0x49 0x69 item traffic is handled by RaceHandler see the opcode pages
class ItemHandler {
public:
    /// sub 4B82A0 only sends after sub 4CB160 refuses an existing shield so this proves the ability ate the hit
    static void handleAbilityFire(Session::Ptr session, Packet& packet, Room* room);

    /// ability class 8 9 fire on every hit 10 11 fire when a real shield pops
    static void handleAbilityClass(Session::Ptr session, Packet& packet, Room* room);
};

} // namespace knc
