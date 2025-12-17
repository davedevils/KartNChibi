/**
 * @file Room.h
 * @brief Game room structure
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>

namespace knc {

class Player;
class Session;

enum class RoomState : uint8_t {
    Waiting = 0,
    Starting = 1,
    Loading = 2,
    Racing = 3,
    Results = 4
};

enum class GameMode : uint8_t {
    ItemRace = 0,
    SpeedRace = 1,
    Battle = 2,
    TeamRace = 3,
    Tutorial = 4
};

struct RoomSettings {
    std::string name = "Room";
    std::string password;
    GameMode mode = GameMode::ItemRace;
    uint8_t maxPlayers = 8;
    uint8_t mapId = 1;
    uint8_t laps = 3;
    bool isPrivate = false;
    bool teamMode = false;
};

// Player data within room context
struct RoomPlayer {
    uint32_t sessionId = 0;       // Session ID
    int32_t characterId = 0;      // Character ID from DB
    std::u16string name;          // Character name
    uint8_t slot = 0;             // Room slot (0-7)
    uint8_t team = 0;             // 0=none, 1=red, 2=blue
    bool ready = false;           // Ready status
    bool isHost = false;          // Is room host
    int32_t vehicleTemplateId = 0;  // Equipped vehicle
    int32_t driverId = 1;         // Driver/skin ID
};

class Room {
public:
    Room(uint32_t id, const RoomSettings& settings);
    
    // Info
    uint32_t id() const { return m_id; }
    const std::string& name() const { return m_settings.name; }
    RoomState state() const { return m_state; }
    const RoomSettings& settings() const { return m_settings; }
    RoomSettings& settings() { return m_settings; }
    
    // Players
    bool addPlayer(std::shared_ptr<Session> session, int32_t characterId, 
                   const std::u16string& name, int32_t vehicleTemplateId);
    bool addPlayer(std::shared_ptr<Session> session);  // Legacy compatibility
    void removePlayer(uint32_t sessionId);
    size_t playerCount() const { return m_sessions.size(); }
    bool isFull() const { return m_sessions.size() >= m_settings.maxPlayers; }
    bool isEmpty() const { return m_sessions.empty(); }
    
    // Player lookup
    RoomPlayer* getPlayer(uint32_t sessionId);
    const RoomPlayer* getPlayer(uint32_t sessionId) const;
    RoomPlayer* getPlayerByCharacter(int32_t characterId);
    std::vector<RoomPlayer> getPlayers() const;
    
    // Ready system
    void setPlayerReady(uint32_t sessionId, bool ready);
    bool isPlayerReady(uint32_t sessionId) const;
    bool areAllPlayersReady() const;  // Excludes host
    int readyCount() const;
    
    // Team
    void setPlayerTeam(uint32_t sessionId, uint8_t team);
    
    // Host
    uint32_t hostSessionId() const { return m_hostSessionId; }
    uint32_t hostId() const { return m_hostSessionId; }  // Alias for compatibility
    int32_t hostCharacterId() const;
    void setHost(uint32_t sessionId);
    bool isHost(uint32_t sessionId) const { return m_hostSessionId == sessionId; }
    
    // Slots
    uint8_t findEmptySlot() const;
    
    // State
    void setState(RoomState state);
    bool canStart() const;
    bool isPlaying() const { return m_state == RoomState::Racing || m_state == RoomState::Loading; }
    bool isWaiting() const { return m_state == RoomState::Waiting; }
    
    // Broadcast
    void broadcast(const class Packet& packet);
    void broadcastExcept(const class Packet& packet, uint32_t excludeSessionId);
    
    // Get sessions
    const std::vector<std::shared_ptr<Session>>& sessions() const { return m_sessions; }

private:
    uint32_t m_id;
    RoomSettings m_settings;
    RoomState m_state = RoomState::Waiting;
    uint32_t m_hostSessionId = 0;
    
    std::vector<std::shared_ptr<Session>> m_sessions;
    std::unordered_map<uint32_t, RoomPlayer> m_players;  // sessionId -> RoomPlayer
};

} // namespace knc

