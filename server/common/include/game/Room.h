/**
 * @file Room.h
 * @brief Game room structure
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

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

class Room {
public:
    Room(uint32_t id, const RoomSettings& settings);
    
    // Info
    uint32_t id() const { return m_id; }
    const std::string& name() const { return m_settings.name; }
    RoomState state() const { return m_state; }
    const RoomSettings& settings() const { return m_settings; }
    
    // Players
    bool addPlayer(std::shared_ptr<Session> session);
    void removePlayer(uint32_t playerId);
    size_t playerCount() const { return m_sessions.size(); }
    bool isFull() const { return m_sessions.size() >= m_settings.maxPlayers; }
    bool isEmpty() const { return m_sessions.empty(); }
    
    // Host
    uint32_t hostId() const { return m_hostId; }
    void setHost(uint32_t id) { m_hostId = id; }
    
    // State
    void setState(RoomState state);
    bool canStart() const;
    
    // Broadcast
    void broadcast(const class Packet& packet);
    void broadcastExcept(const class Packet& packet, uint32_t excludeId);
    
    // Get sessions
    const std::vector<std::shared_ptr<Session>>& sessions() const { return m_sessions; }

private:
    uint32_t m_id;
    RoomSettings m_settings;
    RoomState m_state = RoomState::Waiting;
    uint32_t m_hostId = 0;
    
    std::vector<std::shared_ptr<Session>> m_sessions;
};

} // namespace knc

