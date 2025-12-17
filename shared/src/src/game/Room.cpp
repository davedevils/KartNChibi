/**
 * @file Room.cpp
 * @brief Room implementation
 */

#include "game/Room.h"
#include "net/Session.h"
#include "net/Packet.h"
#include "logging/Logger.h"
#include <algorithm>

namespace knc {

Room::Room(uint32_t id, const RoomSettings& settings)
    : m_id(id), m_settings(settings)
{
}

bool Room::addPlayer(std::shared_ptr<Session> session, int32_t characterId, 
                     const std::u16string& name, int32_t vehicleTemplateId) {
    if (isFull()) return false;
    
    m_sessions.push_back(session);
    
    // Create room player data
    RoomPlayer player;
    player.sessionId = session->id();
    player.characterId = characterId;
    player.name = name;
    player.slot = findEmptySlot();
    player.team = 0;
    player.ready = false;
    player.vehicleTemplateId = vehicleTemplateId;
    
    // First player becomes host
    if (m_sessions.size() == 1) {
        m_hostSessionId = session->id();
        player.isHost = true;
    }
    
    m_players[session->id()] = player;
    
    LOG_INFO("ROOM", "Player joined room " + std::to_string(m_id) + 
             ": charId=" + std::to_string(characterId) + 
             " slot=" + std::to_string(player.slot) +
             " isHost=" + std::to_string(player.isHost));
    
    return true;
}

// Legacy compatibility
bool Room::addPlayer(std::shared_ptr<Session> session) {
    return addPlayer(session, session->characterId, session->characterName, 0);
}

void Room::removePlayer(uint32_t sessionId) {
    // Remove from sessions vector
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [sessionId](const auto& s) { return s->id() == sessionId; }),
        m_sessions.end()
    );
    
    // Remove from players map
    m_players.erase(sessionId);
    
    // Reassign host if needed
    if (m_hostSessionId == sessionId && !m_sessions.empty()) {
        m_hostSessionId = m_sessions.front()->id();
        if (m_players.count(m_hostSessionId)) {
            m_players[m_hostSessionId].isHost = true;
        }
        LOG_INFO("ROOM", "Host transferred to session " + std::to_string(m_hostSessionId));
    }
}

RoomPlayer* Room::getPlayer(uint32_t sessionId) {
    auto it = m_players.find(sessionId);
    return (it != m_players.end()) ? &it->second : nullptr;
}

const RoomPlayer* Room::getPlayer(uint32_t sessionId) const {
    auto it = m_players.find(sessionId);
    return (it != m_players.end()) ? &it->second : nullptr;
}

RoomPlayer* Room::getPlayerByCharacter(int32_t characterId) {
    for (auto& [sid, player] : m_players) {
        if (player.characterId == characterId) {
            return &player;
        }
    }
    return nullptr;
}

std::vector<RoomPlayer> Room::getPlayers() const {
    std::vector<RoomPlayer> result;
    result.reserve(m_players.size());
    for (const auto& [sid, player] : m_players) {
        result.push_back(player);
    }
    // Sort by slot
    std::sort(result.begin(), result.end(), 
              [](const RoomPlayer& a, const RoomPlayer& b) { return a.slot < b.slot; });
    return result;
}

void Room::setPlayerReady(uint32_t sessionId, bool ready) {
    auto it = m_players.find(sessionId);
    if (it != m_players.end()) {
        it->second.ready = ready;
        LOG_INFO("ROOM", "Player " + std::to_string(it->second.characterId) + 
                 " ready=" + std::to_string(ready));
    }
}

bool Room::isPlayerReady(uint32_t sessionId) const {
    auto it = m_players.find(sessionId);
    return (it != m_players.end()) ? it->second.ready : false;
}

bool Room::areAllPlayersReady() const {
    for (const auto& [sid, player] : m_players) {
        // Host doesn't need to be ready
        if (!player.isHost && !player.ready) {
            return false;
        }
    }
    return true;
}

int Room::readyCount() const {
    int count = 0;
    for (const auto& [sid, player] : m_players) {
        if (player.ready) count++;
    }
    return count;
}

void Room::setPlayerTeam(uint32_t sessionId, uint8_t team) {
    auto it = m_players.find(sessionId);
    if (it != m_players.end()) {
        it->second.team = team;
    }
}

int32_t Room::hostCharacterId() const {
    auto it = m_players.find(m_hostSessionId);
    return (it != m_players.end()) ? it->second.characterId : 0;
}

void Room::setHost(uint32_t sessionId) {
    // Remove host flag from old host
    auto oldHost = m_players.find(m_hostSessionId);
    if (oldHost != m_players.end()) {
        oldHost->second.isHost = false;
    }
    
    // Set new host
    m_hostSessionId = sessionId;
    auto newHost = m_players.find(sessionId);
    if (newHost != m_players.end()) {
        newHost->second.isHost = true;
    }
}

uint8_t Room::findEmptySlot() const {
    bool slots[8] = {false};
    for (const auto& [sid, player] : m_players) {
        if (player.slot < 8) {
            slots[player.slot] = true;
        }
    }
    for (uint8_t i = 0; i < 8; ++i) {
        if (!slots[i]) return i;
    }
    return 0;  // Fallback
}

void Room::setState(RoomState state) {
    m_state = state;
    LOG_INFO("ROOM", "Room " + std::to_string(m_id) + " state -> " + std::to_string(static_cast<int>(state)));
}

bool Room::canStart() const {
    if (m_state != RoomState::Waiting) return false;
    if (m_sessions.size() < 1) return false;
    
    // For tutorial mode, 1 player is enough
    if (m_settings.mode == GameMode::Tutorial) return true;
    
    // For other modes, need at least 2 players and all non-host ready
    if (m_sessions.size() < 2) return false;
    
    return areAllPlayersReady();
}

void Room::broadcast(const Packet& packet) {
    auto data = packet.serialize();
    for (auto& session : m_sessions) {
        session->send(data);
    }
}

void Room::broadcastExcept(const Packet& packet, uint32_t excludeSessionId) {
    auto data = packet.serialize();
    for (auto& session : m_sessions) {
        if (session->id() != excludeSessionId) {
            session->send(data);
        }
    }
}

} // namespace knc

