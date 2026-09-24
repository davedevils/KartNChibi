#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class KeepaliveAnticheatHandler {
public:
    /// FUN 00485290 client's 15000ms timer echo throttled to once per 60s to update last seen no reply sent
    static void handleDelayedAckFire(Session::Ptr session, Packet& packet);

    /// sends S2C 0x4D and 0x4E as zero byte probes arming the client's anti cheat reply and round trip timer
    static void armPostLogin(Session::Ptr session);
};

} // namespace knc
