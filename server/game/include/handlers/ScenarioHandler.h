/// quest mode stage 22 ScenarioMenu and stage 17 Quest game
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <string>
#include <vector>

namespace knc {

class GameServer;

class ScenarioHandler {
public:
    /// login frames 0x00F3 defs when the catalogues go out and the 0x00F4 rows of a character
    static std::vector<Packet> loginFrames(uint32_t characterId, bool withDefs);

    /// C2S 0x011C menu open answers 0x00F4 then S2C 0x011C which pushes stage 22
    static void handleMenuOpen(Session::Ptr session, GameServer* server);

    /// C2S 0xF5 Start click the gate the fee the rival ghost then S2C 0xF5 opens stage 17
    static void handleScenarioStageSelect(Session::Ptr session, Packet& packet, GameServer* server);

    /// C2S 0xF8 race time report always answers S2C 0xF8 so the client leaves state 2013
    static void handleScenarioResultReport(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc
