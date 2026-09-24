/// the licence screen and the licence test pass
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>

namespace knc {

class GameServer;

class LicenseHandler {
public:
    /// C2S 0x16 licence screen open the test definitions the progress rows then the ack
    static void handleOpenLicenseScreen(Session::Ptr session, GameServer* server);
    /// C2S 0xA3 test pass the definition row sets the pay and each key pays once
    static void handleLicenseComplete(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc

