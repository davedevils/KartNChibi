/**
 * @file GhostHandler.h
 * @brief Handles ghost mode packets (time attack with ghost replays)
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>
#include <string>

namespace knc {

class GameServer;

struct GhostRecord {
    int32_t id;
    int32_t characterId;
    int32_t mapId;
    int32_t time;          // Time in milliseconds
    std::string replayData; // Replay file path or encoded data
    std::string playerName;
    int32_t vehicleId;
    int32_t driverId;
};

class GhostHandler {
public:
    // Ghost Mode menu
    static void handleOpenGhostMenu(Session::Ptr session, GameServer* server);
    static void handleSelectMap(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Ghost race
    static void handleStartGhostRace(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleGhostRaceComplete(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleSaveGhost(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Ghost data
    static void handleGetGhostList(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleDownloadGhost(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Helpers
    static std::vector<GhostRecord> getGhostsForMap(int32_t mapId, int limit = 10);
    static GhostRecord getBestGhost(int32_t mapId);
    static bool saveGhostRecord(int32_t characterId, int32_t mapId, int32_t time, 
                                const std::string& replayData, int32_t vehicleId, int32_t driverId);
};

} // namespace knc

