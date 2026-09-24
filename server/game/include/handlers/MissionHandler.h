/// the mission menu start and finish of the stock mission stages 24 and 25
#pragma once
#include "packets/gen/MissionPackets.h"
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>

namespace knc {

class GameServer;

class MissionHandler {
public:
    /// C2S 0x8F the 0x87 definitions once the 0x88 progress rows then the ack that opens stage 24
    static void handleOpenMissionMenu(Session::Ptr session, GameServer* server);
    /// C2S 0x90 checks the row is playable takes the fee and arms the run the finish must match
    static void handleStartMission(Session::Ptr session, const MissionPackets::MissionIdReq& req,
                                   GameServer* server);
    /// C2S 0x8C pays only a run this server started and only on the first clear
    static void handleMissionComplete(Session::Ptr session, const MissionPackets::MissionIdReq& req,
                                      GameServer* server);
};

} // namespace knc
