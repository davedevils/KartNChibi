/// garage open and the fallback of the 0xB9 0xBA verbs InventoryHandler did not claim
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>

namespace knc {

class GameServer;

class GarageHandler {
public:
    /// C2S 0x0F a working server answers with the 0x0F show alone
    static void handleOpenGarage(Session::Ptr session, Packet& packet, GameServer* server);
    /// 0xB9 and 0xBA reach here only for a category InventoryHandler does not own they are logged
    static void handleGarageInstall(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleGarageRemove(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc
