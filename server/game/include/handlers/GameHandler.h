/**
 * @file GameHandler.h
 * @brief Handles in-race packets (position, items, finish)
 */

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class GameHandler {
public:
    void handlePosition(Session::Ptr session, Packet& packet, GameServer* server);
    void handleItemUse(Session::Ptr session, Packet& packet, GameServer* server);
    void handleFinish(Session::Ptr session, Packet& packet, GameServer* server);
    void handleLapComplete(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc
