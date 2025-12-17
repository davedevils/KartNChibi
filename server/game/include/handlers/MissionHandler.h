/**
 * @file MissionHandler.h
 * @brief Handles mission/quest packets
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>

namespace knc {

class GameServer;

struct MissionData {
    int32_t id;
    int32_t currentProgress;
    int32_t targetCount;
    bool isCompleted;
};

class MissionHandler {
public:
    // Mission list/info
    static void handleGetMissionList(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleGetMissionDetails(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Mission completion
    static void handleClaimReward(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Progress tracking (called internally by other handlers)
    static void updateRaceProgress(int characterId, bool won);
    static void updateItemProgress(int characterId, int itemCount);
    static void updateTimeProgress(int characterId, int mapId, int timeMs);
    
    // Helpers
    static std::vector<MissionData> getPlayerMissions(int characterId, const std::string& category);
    static void sendMissionList(Session::Ptr session, int characterId);
    static void sendMissionComplete(Session::Ptr session, int missionId);
};

} // namespace knc

