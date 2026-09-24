
#include "game/Room.h"
#include <random>
#include "net/Session.h"
#include "net/Packet.h"
#include "logging/Logger.h"
#include <algorithm>
#include <set>

namespace knc {

Room::Room(uint32_t id, const RoomSettings& settings)
    : m_id(id), m_settings(settings)
{
}

bool Room::addPlayer(std::shared_ptr<Session> session, int32_t characterId, 
                     const std::u16string& name, int32_t vehicleTemplateId) {
    if (!session) return false;
    // a second seat for the same socket doubles the racer on the grid and never shows twice in the room
    if (m_players.count(session->id()) != 0) return false;
    if (isFull()) return false;

    // the seat must be free of humans and of bots or the client drops the second 0x21
    const uint8_t slot = findEmptySlot();
    if (slot == kNoSlot) return false;

    m_sessions.push_back(session);

    RoomPlayer player;
    player.sessionId = session->id();
    player.characterId = characterId;
    player.name = name;
    player.slot = slot;
    player.team = isTeamMode() ? balancedTeam() : 0;
    player.ready = false;
    player.vehicleTemplateId = vehicleTemplateId;
    
    // first player becomes host
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

bool Room::addPlayer(std::shared_ptr<Session> session) {
    return addPlayer(session, session->characterId, session->characterName, 0);
}

void Room::removePlayer(uint32_t sessionId) {
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [sessionId](const auto& s) { return s->id() == sessionId; }),
        m_sessions.end()
    );

    m_players.erase(sessionId);

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
    // bots need this row too or no client ever spawns their car
    for (auto& b : m_bots) {
        if (b.characterId == characterId) return &b;
    }
    return nullptr;
}

std::vector<RoomPlayer> Room::getPlayers() const {
    std::vector<RoomPlayer> result;
    result.reserve(m_players.size());
    for (const auto& [sid, player] : m_players) {
        result.push_back(player);
    }
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

uint8_t Room::balancedTeam() const {
    int red = 0;
    int blue = 0;
    for (const auto& kv : m_players) {
        if (kv.second.team == 1) ++red;
        else if (kv.second.team == 2) ++blue;
    }
    for (const auto& b : m_bots) {
        if (b.team == 1) ++red;
        else if (b.team == 2) ++blue;
    }
    return red <= blue ? 1 : 2;
}

int32_t Room::hostCharacterId() const {
    auto it = m_players.find(m_hostSessionId);
    return (it != m_players.end()) ? it->second.characterId : 0;
}

void Room::setHost(uint32_t sessionId) {
    auto oldHost = m_players.find(m_hostSessionId);
    if (oldHost != m_players.end()) {
        oldHost->second.isHost = false;
    }

    m_hostSessionId = sessionId;
    auto newHost = m_players.find(sessionId);
    if (newHost != m_players.end()) {
        newHost->second.isHost = true;
    }
}

uint8_t Room::findEmptySlot() const {
    return nextFreeSlot();
}

void Room::setState(RoomState state) {
    m_state = state;
    LOG_INFO("ROOM", "Room " + std::to_string(m_id) + " state -> " + std::to_string(static_cast<int>(state)));
}

const char* Room::startRefusal() const {
    if (m_state != RoomState::Waiting || m_sessions.empty()) return "MSG_NOT_AVAILABLE_START";

    // sub 40C950 counts every 0x21 member so bots count as members and as a side
    const std::vector<RoomPlayer> members = participants();
    const bool speed = m_settings.mode == GameMode::SpeedSingle || m_settings.mode == GameMode::SpeedTeam;
    if (speed && members.size() > 16) return "MSG_MAX_ROOM_USER_16";
    if (!speed && members.size() > 8) return "MSG_MAX_ROOM_USER_8";
    if (isTeamMode()) {
        size_t red = 0;
        size_t blue = 0;
        for (const auto& m : members) {
            if (m.team == 1) ++red;
            else if (m.team == 2) ++blue;
        }
        if (red != blue) return "MSG_NOT_BALLENCE_START";
        if (members.size() < 4) return "MSG_SMALL_MEMBER_ERROR";
    }

    // the alone guard of the solo modes stays with the stock client so one tester can still race
    if (m_sessions.size() == 1) return nullptr;
    return areAllPlayersReady() ? nullptr : "MSG_NOT_AVAILABLE_START";
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

// bot filler was rewritten after the 21 aug checkout following the surviving header

uint8_t Room::nextFreeSlot() const {
    const uint8_t cap = seatCap();
    for (uint8_t slot = 0; slot < cap; ++slot) {
        bool taken = false;
        for (const auto& kv : m_players) {
            if (kv.second.slot == slot) { taken = true; break; }
        }
        if (!taken) {
            for (const auto& b : m_bots) {
                if (b.slot == slot) { taken = true; break; }
            }
        }
        if (!taken) return slot;
    }
    // no free seat a reused slot means two members on one seat and two cars on one grid row
    return kNoSlot;
}

int Room::addBots(int count) {
    int added = 0;
    for (int i = 0; i < count; ++i) {
        if (isFull()) break;

        const uint8_t slot = nextFreeSlot();
        if (slot == kNoSlot) break;

        RoomPlayer bot;
        bot.isBot = true;
        bot.sessionId = 0;
        // high id so it can never collide with a real character row
        bot.characterId = kBotIdBase + slot;
        // bots pick from drivers 5 to 14 and karts 10010 to 10015 which all ship working models
        static const char* const kNames[] = {
            "Ace", "Blitz", "Comet", "Dash", "Echo", "Flash", "Gizmo", "Havoc",
            "Ivy", "Jolt", "Koda", "Luna", "Mochi", "Nova", "Orbit", "Pixel",
            "Quill", "Rocket", "Sky", "Turbo", "Vex", "Wren", "Yumi", "Zed" };
        static const int32_t kDrivers[] = { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
        static const int32_t kKarts[]   = { 10010, 10011, 10012, 10013, 10014, 10015 };
        std::mt19937 rng(static_cast<uint32_t>(m_id * 7919u + slot * 104729u +
                         static_cast<uint32_t>(std::random_device{}())));
        std::string name;
        for (int tries = 0; tries < 48; ++tries) {
            name = kNames[rng() % (sizeof(kNames) / sizeof(kNames[0]))];
            bool taken = false;
            for (const auto& other : m_bots) {
                if (std::string(other.name.begin(), other.name.end()) == name) { taken = true; break; }
            }
            if (!taken) break;
        }
        bot.name.assign(name.begin(), name.end());
        bot.slot = slot;
        bot.team = isTeamMode() ? balancedTeam() : 0;
        bot.ready = true;   // a bot is always ready or the host can never start
        bot.isHost = false;
        bot.vehicleTemplateId = kKarts[rng() % (sizeof(kKarts) / sizeof(kKarts[0]))];
        bot.driverId = kDrivers[rng() % (sizeof(kDrivers) / sizeof(kDrivers[0]))];

        m_bots.push_back(bot);
        ++added;
    }
    if (added > 0) {
        LOG_INFO("ROOM", "Room " + std::to_string(m_id) + " added " +
                 std::to_string(added) + " bots");
    }
    return added;
}

int Room::fillWithBotsIfEnabled() {
    if (!m_settings.fillWithBots) return 0;
    const int need = static_cast<int>(seatCap()) - static_cast<int>(playerCount());
    return need > 0 ? addBots(need) : 0;
}

void Room::clearBots() {
    m_bots.clear();
}

std::vector<RoomPlayer> Room::participants() const {
    std::vector<RoomPlayer> out;
    out.reserve(m_players.size() + m_bots.size());
    // one row per character or the race spawns the same car twice and the room shows it once
    std::set<int32_t> seen;
    for (const auto& kv : m_players) {
        if (seen.insert(kv.second.characterId).second) out.push_back(kv.second);
    }
    for (const auto& b : m_bots) {
        if (seen.insert(b.characterId).second) out.push_back(b);
    }
    std::sort(out.begin(), out.end(),
              [](const RoomPlayer& a, const RoomPlayer& b) {
                  if (a.slot != b.slot) return a.slot < b.slot;
                  return a.characterId < b.characterId;
              });
    return out;
}

} // namespace knc

