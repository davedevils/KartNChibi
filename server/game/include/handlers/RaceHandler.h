#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>

namespace knc {

class GameServer;
class Room;

struct RacePlayer {
    int32_t playerId = 0;
    uint8_t position = 0;
    uint8_t lap = 0;
    int32_t lastLapTime = 0;
    int32_t totalTime = 0;
    int32_t score = 0;
    bool finished = false;
    float x = 0, y = 0, z = 0, rot = 0;
    std::chrono::steady_clock::time_point lastUpdate;
    int32_t suspiciousCount = 0;  // Anti-cheat counter
    std::vector<int32_t> lapTimes;  // Track all lap times
    
    // Mini Turbo state
    uint8_t boostLevel = 0;        // 0=none, 1=blue, 2=orange, 3=red (max)
    bool isBoosting = false;       // Currently in boost
    std::chrono::steady_clock::time_point boostStart;  // When boost started
    int32_t boostCount = 0;        // Total boosts used in race
};

class RaceHandler {
public:
    // race flow
    void handleStartRace(Session::Ptr session, Room* room, GameServer* server);
    void handlePosition(Session::Ptr session, Packet& packet, Room* room);
    void handleLapComplete(Session::Ptr session, Packet& packet, Room* room);
    void handleFinish(Session::Ptr session, Packet& packet, Room* room, GameServer* server);
    void handleItemPickup(Session::Ptr session, Packet& packet, Room* room);
    void handleItemUse(Session::Ptr session, Packet& packet, Room* room);
    void handleItemHit(Session::Ptr session, Packet& packet, Room* room);
    
    // Mini Turbo / Boost
    void handleDriftStart(Session::Ptr session, Packet& packet, Room* room);
    void handleDriftEnd(Session::Ptr session, Packet& packet, Room* room);
    void handleBoostActivate(Session::Ptr session, Packet& packet, Room* room);
    void handleBoostEnd(Session::Ptr session, Room* room);
    
    // race management
    void startCountdown(Room* room);
    void startRace(Room* room);
    void endRace(Room* room, GameServer* server);
    void updatePositions(Room* room);
    
    // state
    RacePlayer* getPlayer(uint32_t roomId, int32_t playerId);
    void addPlayer(uint32_t roomId, int32_t playerId);
    void removePlayer(uint32_t roomId, int32_t playerId);
    void clearRoom(uint32_t roomId);
    
private:
    void sendCountdown(Room* room, int32_t seconds);
    void sendRaceStart(Room* room);
    void sendResults(Room* room);
    void calculatePositions(uint32_t roomId);
    
    // roomId -> players
    std::unordered_map<uint32_t, std::vector<RacePlayer>> m_racePlayers;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_raceStartTimes;
    mutable std::mutex m_raceMutex;  // Protects m_racePlayers and m_raceStartTimes
};

} // namespace knc

