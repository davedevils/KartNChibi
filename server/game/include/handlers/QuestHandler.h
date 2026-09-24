/// quest rows append only via sub 452180 must stay contiguous or sub 452240 and sub 402210 crash on null

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/QuestPackets.h"

#include <cstdint>
#include <vector>

namespace knc {

class GameServer;

/// quest handler is static with the catalog cached process wide
class QuestHandler {
public:
    /// publishes the unlocked catalog and player state once per enter catalog rows first then one state list
    static void onCharacterEnter(Session::Ptr session, GameServer* server);

    /// c2s 0xFE accept quest 4 bytes answers s2c 0xFE only on success
    static void handleAccept(Session::Ptr session, Packet& packet, GameServer* server);

    /// c2s 0x100 discard quest 4 bytes answers s2c 0x100 only on success
    static void handleDiscard(Session::Ptr session, Packet& packet, GameServer* server);

    /// c2s 0x102 progress report 4 bytes only for quest index 0 to 3 hard coded to race events
    static void handleProgressReport(Session::Ptr session, Packet& packet, GameServer* server);

    /// sends s2c 0xFB four times for one theme step append only returns count sent
    static size_t publishTheme(Session::Ptr session, uint32_t themeId);

    /// reloads and revalidates the quest catalog returns false and empties it if rows break a crash constraint
    static bool reloadCatalog();

    /// copy of the validated catalog empty when db rows were refused
    static std::vector<QuestPackets::QuestDefWire> catalog();
};

} // namespace knc
