/**
 * @file RoomHandler.h
 * @brief Handles room packets (ready, start, leave)
 */

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class RoomHandler {
public:
    void handleReady(Session::Ptr session, Packet& packet, GameServer* server);
    void handleLeaveRoom(Session::Ptr session, GameServer* server);
    void handleStartGame(Session::Ptr session, GameServer* server);
    void handleTeamChange(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc
