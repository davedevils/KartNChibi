/**
 * @file ScenarioHandler.h
 * @brief Handles scenario/story mode packets
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>

namespace knc {

class GameServer;

struct ScenarioProgress {
    int32_t characterId;
    int32_t currentChapter;
    int32_t currentStage;
    int32_t starsEarned;      // 0-3 per stage
    int32_t totalStars;
    bool completed;
};

struct ScenarioStage {
    int32_t id;
    int32_t chapter;
    int32_t stage;
    int32_t mapId;
    int32_t difficulty;       // 1-5
    int32_t requiredStars;    // Stars needed to unlock
    std::string name;
    std::string description;
};

class ScenarioHandler {
public:
    // Menu navigation
    static void handleOpenScenarioMenu(Session::Ptr session, GameServer* server);
    static void handleSelectChapter(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleSelectStage(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Gameplay
    static void handleStartScenario(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleScenarioComplete(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Data
    static void handleGetProgress(Session::Ptr session, GameServer* server);
    static void handleGetChapterList(Session::Ptr session, GameServer* server);
    
    // Helpers
    static ScenarioProgress getPlayerProgress(int32_t characterId);
    static std::vector<ScenarioStage> getChapterStages(int32_t chapter);
    static bool unlockNextStage(int32_t characterId);
};

} // namespace knc

