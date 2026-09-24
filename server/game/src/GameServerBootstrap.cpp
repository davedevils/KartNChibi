/// listen accept sessions rooms and the two background timers

#include "GameServer.h"
#include "GameServerInternal.h"
#include "packets/gen/GhostPackets.h"
#include "packets/gen/PartStatPackets.h"
#include "packets/gen/GachaPetPackets.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ShopPackets.h"
#include "packets/gen/MissionPackets.h"
#include "packets/gen/SocialPackets.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ItemPackets.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/QuestHandler.h"
#include "logging/Logger.h"
#include "util/InviteOption.h"
#include <unordered_set>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <array>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <cctype>
#include "security/BanManager.h"
#include "security/PacketValidator.h"
#include "db/Database.h"
#include "handlers/LicenseHandler.h"
#include "handlers/MissionHandler.h"
#include "handlers/AntiCheatHandler.h"
#include "handlers/GarageHandler.h"
#include "handlers/LobbyHandler.h"
#include "handlers/GhostHandler.h"
#include "handlers/ScenarioHandler.h"
#include "handlers/CharCreateHandler.h"
#include "handlers/ItemHandler.h"
#include "handlers/GachaHandler.h"
#include "handlers/KeepaliveAnticheatHandler.h"
#include "crypto/PasswordHash.h"

namespace knc {

// set by main from the ini before run used by the redirect and registration replies
std::string g_advertisedHost = "127.0.0.1";

namespace {

// per session flood throttle config from Protocol h RateLimit constants motion 0x40 has no interval throttle
RateLimiter::Config makeRateLimiterConfig() {
    RateLimiter::Config cfg;
    cfg.globalMaxPerSec = RateLimit::GLOBAL_MAX_PACKETS_SEC;
    cfg.defaultMinIntervalMs = RateLimit::DEFAULT_MIN_INTERVAL;
    cfg.opcodeMinIntervalMs[CMD::C_MOTION] = 0;      // 10 Hz race motion never drops
    cfg.opcodeMinIntervalMs[CMD::C_HEARTBEAT] = 0;
    cfg.opcodeMinIntervalMs[0x0B] = 0;
    cfg.opcodeMinIntervalMs[CMD::C_LOBBY_CHAT] = RateLimit::CHAT_MIN_INTERVAL;
    cfg.opcodeMinIntervalMs[0x67] = 0;   // 0x67 progress lands about once per client frame 0x41 checkpoint bursts on a hairpin
    cfg.opcodeMinIntervalMs[0x41] = 0;
    cfg.opcodeMinIntervalMs[GhostPackets::kOpReplayCount] = 0;
    cfg.opcodeMinIntervalMs[GhostPackets::kOpReplayChunk] = 0;  // replay upload is one packet per stage tick about 16 ms so no interval floor
    cfg.opcodeMinIntervalMs[CMD::C_WHISPER] = RateLimit::CHAT_MIN_INTERVAL;
    // two 0xB9 in one write land inside the 50 ms floor and the second one was dropped as a flood
    cfg.opcodeMinIntervalMs[CMD::C_GARAGE_INSTALL] = 0;
    cfg.opcodeMinIntervalMs[CMD::C_GARAGE_REMOVE] = 0;
    // FUN 00402210 answers each S2C 0x0D the grid one and the GO one come a frame apart
    cfg.opcodeMinIntervalMs[0x0D] = 0;
    return cfg;
}

}  // namespace

std::string toHex(uint8_t v) {
    const char* hex = "0123456789ABCDEF";
    return std::string(1, hex[v >> 4]) + hex[v & 0xF];
}

std::string toHex16(uint16_t v) {
    const char* hex = "0123456789ABCDEF";
    char buf[5] = {
        hex[(v >> 12) & 0xF], hex[(v >> 8) & 0xF],
        hex[(v >> 4) & 0xF],  hex[v & 0xF], '\0'
    };
    return std::string(buf, 4);
}

GameServer::GameServer(int port)
    : m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<uint16_t>(port)))
{
    if (const char* envFile = std::getenv("KNC_LIVENESS_FILE")) {
        if (*envFile) m_listenerHealth.setFile(envFile);
    }
    startAccept();
    startRaceTick();   // motion fan out dead without tick driver solo room never starts so seat bot after the wait
    startMatchTick();
    startLivenessTick();  // a deaf loop must read as deaf not as healthy
}

bool GameServer::listenerAlive(int64_t maxAgeMs) const {
    return !m_listenerHealth.stale(maxAgeMs);
}

bool GameServer::postDb(uint64_t orderKey, std::function<void()> job) {
    return m_dbPool.post(orderKey, std::move(job));
}

void GameServer::postIo(std::function<void()> job) {
    asio::post(m_ioContext, std::move(job));
}

void GameServer::run() {
    // whoever the table says is in game was in game on the last process
    Database::instance().execute("DELETE FROM online_players");
    // KNC DB THREADS 0 keeps every query on the io thread for a bisect
    size_t dbThreads = 4;
    if (const char* env = std::getenv("KNC_DB_THREADS")) {
        const int want = std::atoi(env);
        dbThreads = want < 0 ? 0 : static_cast<size_t>(want);
    }
    if (dbThreads > 0) m_dbPool.start(dbThreads);
    LOG_INFO("GAME", "Server running...");

    // an escaped throw used to end the loop and take the listener with it now the loop restarts
    for (;;) {
        try {
            m_ioContext.run();
            return;
        } catch (const std::exception& e) {
            LOG_ERROR("GAME", std::string("loop caught an escaped error: ") + e.what());
        } catch (...) {
            LOG_ERROR("GAME", "loop caught an escaped unknown error");
        }
        if (m_ioContext.stopped()) return;
    }
}

void GameServer::stop() {
    m_dbPool.stop();
    m_ioContext.stop();
    LOG_INFO("GAME", "Server stopped");
}

void GameServer::startAccept() {
    m_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        m_listenerHealth.touch();

        if (ec) {
            if (acceptErrorIsFatal(ec)) {
                LOG_ERROR("GAME", "acceptor is gone: " + ec.message());
                return;
            }
            const int delay = acceptRetryDelayMs(ec);
            LOG_WARN("GAME", "accept failed: " + ec.message() +
                     " retry in " + std::to_string(delay) + " ms");
            if (delay > 0) {
                // out of descriptors arming at once would spin hot on the same error
                m_acceptRetryTimer.expires_after(std::chrono::milliseconds(delay));
                m_acceptRetryTimer.async_wait([this](const std::error_code& wec) {
                    if (!wec) startAccept();
                });
                return;
            }
            startAccept();
            return;
        }

        try {
            acceptOne(std::move(socket));
        } catch (const std::exception& e) {
            LOG_ERROR("GAME", std::string("accept path threw: ") + e.what());
        } catch (...) {
            LOG_ERROR("GAME", "accept path threw an unknown error");
        }

        startAccept();
    });
}

void GameServer::acceptOne(asio::ip::tcp::socket socket) {
    std::error_code rec;
    const auto peer = socket.remote_endpoint(rec);
    if (rec) {
        // the peer left between the accept and the greeting this used to throw out of the loop
        LOG_WARN("GAME", "peer gone before the greeting: " + rec.message());
        socket.close(rec);
        return;
    }
    const std::string ip = peer.address().to_string();

    if (BanManager::instance().isBanned(ip)) {
        LOG_WARN("GAME", "Rejected banned IP: " + ip);
        socket.close(rec);
        return;
    }

    auto session = std::make_shared<Session>(std::move(socket));
    LOG_INFO("GAME", "New connection from " + session->remoteAddress() + " (ID: " + std::to_string(session->id()) + ")");

    session->setPacketHandler([this](Session::Ptr s, Packet& pkt) {
        handlePacket(s, pkt);
    });

    session->setDisconnectHandler([this](Session::Ptr s) {
        onDisconnect(s);
    });

    addSession(session);
    session->setIdleTimeout(std::chrono::seconds(SESSION_IDLE_LIMIT_SEC));
    session->start();

    // the account arrives on 0xA7 with a login token or on 0x07 with credentials from a tool that skipped login
    LOG_INFO("GAME", "Client initialized: " + session->remoteAddress());
}

void GameServer::startLivenessTick() {
    m_livenessTimer.expires_after(std::chrono::seconds(5));
    m_livenessTimer.async_wait([this](const std::error_code& ec) {
        if (ec) return;
        if (!m_acceptor.is_open()) {
            LOG_ERROR("GAME", "acceptor is closed the server takes no new client");
        } else {
            m_listenerHealth.touch();
            m_listenerHealth.flush();
        }
        // one line a minute so a frozen loop is visible in the container log
        if (++m_livenessTicks % 12 == 0) {
            LOG_INFO("GAME", "accept loop alive, sessions " + std::to_string(sessionCount()));
        }
        startLivenessTick();
    });
}

void GameServer::startMatchTick() {
    m_matchTickTimer.expires_after(std::chrono::seconds(5));
    m_matchTickTimer.async_wait([this](const std::error_code& ec) {
        if (ec) return;
        const int64_t now = static_cast<int64_t>(RaceHandler::nowMs());
        std::vector<std::shared_ptr<Room>> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_roomsMutex);
            snapshot.reserve(m_rooms.size());
            for (const auto& kv : m_rooms) snapshot.push_back(kv.second);
        }
        for (const auto& room : snapshot) {
            if (!room) continue;
            const uint32_t id = room->id();

            // nothing client side leaves the race stage once the podium ends so the room screen must go back out
            if (room->resultsShownMs != 0 && now - room->resultsShownMs >= kResultsBoardMs) {
                room->resultsShownMs = 0;
                for (const auto& s : room->sessions()) sendRoomScreen(s, room.get());
                LOG_INFO("ROOM", "result board done, room screen resent for room " +
                         std::to_string(id));
            }
            if (room->state() != RoomState::Waiting || room->sessions().empty()) {
                m_roomWaitSince.erase(id);
                m_randomInviteDue.erase(id);
                continue;
            }
            // the podium still holds the screen a 0x21 sent now lands on a room the client has not entered
            if (room->resultsShownMs != 0) { m_roomWaitSince.erase(id); m_randomInviteDue.erase(id); continue; }
            // a room with a second human needs no help the invited human came so the CPU car is not needed
            if (room->sessions().size() > 1) { m_roomWaitSince.erase(id); m_randomInviteDue.erase(id); continue; }
            if (room->playerCount() >= room->settings().maxPlayers) continue;

            // a 0x12F asked a human the exe box closes after 5 s so take the CPU car when nobody came
            auto inv = m_randomInviteDue.find(id);
            if (inv != m_randomInviteDue.end()) {
                if (now < inv->second) continue;
                m_randomInviteDue.erase(inv);
                if (room->bots().size() < kMatchMaxBots) {
                    seatOneBot(room.get(), "random invite unanswered");
                    m_roomWaitSince[id] = now;
                    continue;
                }
            }

            auto it = m_roomWaitSince.find(id);
            if (it == m_roomWaitSince.end()) { m_roomWaitSince[id] = now; continue; }
            if (now - it->second < kMatchWaitSeconds * 1000) continue;
            // the room fills one CPU car per wait up to the cap matching the seats the room shows
            if (room->bots().size() >= kMatchMaxBots) continue;

            const size_t before = room->bots().size();
            room->addBots(1);
            if (room->bots().size() == before) continue;
            const RoomPlayer& b = room->bots().back();
            room->broadcast(PacketBuilder::roomSlotEnabled(b.slot, true));
            room->broadcast(PacketBuilder::roomMember(buildBotMember(b)));
            m_roomWaitSince[id] = now;
            LOG_INFO("ROOM", "match wait elapsed, seated a bot in slot " +
                     std::to_string(b.slot) + " of room " + std::to_string(id));
        }
        startMatchTick();
    });
}

void GameServer::startRaceTick() {
    m_raceTickTimer.expires_after(std::chrono::milliseconds(50));
    m_raceTickTimer.async_wait([this](const std::error_code& ec) {
        if (ec) return;
        m_raceHandler.tick(RaceHandler::nowMs(), this);
        startRaceTick();
    });
}

void GameServer::onDisconnect(Session::Ptr session) {
    LOG_INFO("GAME", "Client disconnected: " + session->remoteAddress() + " (ID: " + std::to_string(session->id()) + ")");
    try {

    // the presence row clears only when still this socket's a rebind already zeroed the old one
    if (session->accountId != 0) {
        Database::instance().executePrepared(
            "DELETE FROM online_players WHERE account_id = ? AND game_session = ?",
            {static_cast<int32_t>(session->accountId), static_cast<int32_t>(session->id())});
    }

    // per session mirrors leak one set per disconnect without this
    m_socialHandler.onSessionClosed(session->id());
    m_carCraftHandler.forgetSession(session->id());
    GhostHandler::onDisconnect(session);
    AntiCheatHandler::clearViolations(session->id());

    auto room = getRoom(session->roomId);
    if (room) {
        room->removePlayer(session->id());
        // dnf cleanup else race hangs on gone player
        m_raceHandler.handlePlayerLeave(room.get(), session->characterId, this);
        if (!room->isEmpty()) {
            room->broadcast(PacketBuilder::playerLeft(session->characterId));
        }
    }

    std::vector<uint32_t> died;
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (auto it = m_rooms.begin(); it != m_rooms.end(); ) {
            if (it->second->isEmpty()) {
                LOG_INFO("GAME", "Removing empty room: " + std::to_string(it->first));
                died.push_back(it->first);
                it = m_rooms.erase(it);
            } else {
                ++it;
            }
        }
    }

    removeSession(session->id());
    // the grid keeps a dead row forever unless this clears it
    for (uint32_t id : died) broadcastLobbyRoomRemove(id);
    for (const auto& s : getLobbySessions()) sendLobbyRoomList(s);
    } catch (const std::exception& e) {
        // a cleanup throw must never reach the loop the session row is dropped anyway
        LOG_ERROR("GAME", std::string("disconnect cleanup threw: ") + e.what());
        removeSession(session->id());
    } catch (...) {
        LOG_ERROR("GAME", "disconnect cleanup threw an unknown error");
        removeSession(session->id());
    }
}

std::shared_ptr<Room> GameServer::createRoom(const RoomSettings& settings) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    auto room = std::make_shared<Room>(m_nextRoomId++, settings);
    m_rooms[room->id()] = room;
    LOG_INFO("GAME", "Created room: " + settings.name + " (ID: " + std::to_string(room->id()) + ")");
    return room;
}

std::shared_ptr<Room> GameServer::getRoom(uint32_t roomId) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    auto it = m_rooms.find(roomId);
    return (it != m_rooms.end()) ? it->second : nullptr;
}

void GameServer::removeRoom(uint32_t roomId) {
    // a room that dies mid race left its CPU cars ticking and crossing the line two minutes later
    m_raceHandler.clearRoom(roomId);
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    m_rooms.erase(roomId);
}

void GameServer::addSession(Session::Ptr session) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions[session->id()] = session;
    m_rateLimiters.emplace(session->id(), RateLimiter(makeRateLimiterConfig()));
}

void GameServer::removeSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions.erase(sessionId);
    m_rateLimiters.erase(sessionId);
}

Session::Ptr GameServer::getSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second : nullptr;
}

Session::Ptr GameServer::pickRandomInviteTarget(const Session::Ptr& asker, Room* room) const {
    if (!asker || !room) return nullptr;

    // stock client hides popup for option 11 opted out players C2S 0x0130 float value kept on session and account
    std::vector<Session::Ptr> idle;
    std::vector<Session::Ptr> waiting;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (const auto& kv : m_sessions) {
            const Session::Ptr& s = kv.second;
            if (!s || !inviteCandidate(*s, asker->id(), room->id())) continue;
            if (s->roomId == 0) idle.push_back(s); else waiting.push_back(s);
        }
    }

    // a player standing in the lobby is the cheapest to move so ask that one first
    if (!idle.empty()) return idle[static_cast<size_t>(RaceHandler::nowMs()) % idle.size()];
    if (waiting.empty()) return nullptr;

    // else someone sitting in another room that has not started yet
    std::vector<Session::Ptr> free;
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (const Session::Ptr& s : waiting) {
            auto it = m_rooms.find(s->roomId);
            if (it == m_rooms.end() || !it->second) continue;
            if (it->second->state() != RoomState::Waiting) continue;
            free.push_back(s);
        }
    }
    if (free.empty()) return nullptr;
    return free[static_cast<size_t>(RaceHandler::nowMs()) % free.size()];
}

bool GameServer::seatOneBot(Room* room, const char* why) {
    if (!room) return false;
    const size_t before = room->bots().size();
    room->addBots(1);
    if (room->bots().size() == before) {
        LOG_WARN("ROOM", std::string(why) + " could not seat anyone in room " +
                 std::to_string(room->id()));
        return false;
    }
    const RoomPlayer& b = room->bots().back();
    // 0x32 before 0x21 or sub 40D650 bails on the disabled slot guard
    room->broadcast(PacketBuilder::roomSlotEnabled(b.slot, true));
    room->broadcast(PacketBuilder::roomMember(buildBotMember(b)));
    LOG_INFO("ROOM", std::string(why) + " seated a bot in slot " +
             std::to_string(b.slot) + " of room " + std::to_string(room->id()));
    return true;
}

Session::Ptr GameServer::findSessionByCharacterId(int32_t characterId) const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (const auto& [id, s] : m_sessions) {
        if (s && static_cast<int32_t>(s->characterId) == characterId) {
            return s;
        }
    }
    return nullptr;
}

}  // namespace knc
