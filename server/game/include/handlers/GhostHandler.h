/// ghost records replays and stage 15 and 17 ghosts rewritten after a bad checkout took the header
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/GhostPackets.h"

namespace knc {

class GameServer;

class GhostHandler {
public:
    // opcode probes the dispatcher asks before routing
    static bool isBoardRequest(const Packet& packet);
    static bool isGhostEnter(const Packet& packet);
    static bool isStageEvent(const Packet& packet);
    static bool isSubmit(Session::Ptr session, const Packet& packet);
    static bool isQuestGhostStart(const Packet& packet);

    static void handleMenuSelect(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleGhostEnter(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleStageBegin(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleFinalLap(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleUploadCount(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleUploadChunk(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleSubmit(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleQuestGhostStart(Session::Ptr session, Packet& packet, GameServer* server);

    // the board is about 30kb for 43 tracks the client buffer is 0x2000 so it must go through GameServer sendDripped
    static void sendRecordBoard(Session::Ptr session, bool withPopup = false,
                                GameServer* server = nullptr);
    static size_t sendQuestGhost(Session::Ptr session, uint32_t questIndex);

    static void onDisconnect(Session::Ptr session);

    static std::vector<int32_t> boardTrackOrder();
    static std::string replayDir();
    static std::string exportDir();
    static bool exportRepFile(const std::string& path,
                              const std::vector<ReplayFrame>& frames);
    static bool exportRecordReplay(int32_t trackId, uint32_t charId);
    static bool importRepFile(const std::string& path, int32_t trackId, uint32_t charId,
                              const std::u16string& name, uint32_t totalTimeMs);
    static size_t seedShippedReplays(const std::string& devClientDir);
    static void seedShippedReplaysOnce();
};

}  // namespace knc
