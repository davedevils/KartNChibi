/// chat game shop data and inventory handlers plus the room screen rebuild they share

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

void GameServer::handleWhisper(Session::Ptr session, Packet& packet) {
    m_chatHandler.handleWhisper(session, packet, this);
}

void GameServer::handleLobbyChat(Session::Ptr session, Packet& packet) {
    m_chatHandler.handleLobbyChat(session, packet, this);
}

void GameServer::handleStateChange(Session::Ptr session, Packet& packet) {
    // sub 408EE0 C2S 0x2C is QUICK MATCH not a click kind is the room mode every button raises MSG WAIT
    int32_t kind = 0;
    int32_t sub  = 0;
    if (packet.remaining() >= 8) {
        kind = packet.readInt32();
        sub  = packet.readInt32();
    }
    LOG_INFO("GAME", "quick match request mode " + std::to_string(kind) +
             " sub " + std::to_string(sub) + " from " + session->remoteAddress());
    quickMatch(session, kind);
}

void GameServer::quickMatch(Session::Ptr session, int32_t kind) {
    if (!session) return;
    if (kind < 0 || kind > 3) {
        // dead button id 4 sends 0 3 and is never registered in this build
        LOG_WARN("GAME", "quick match unreachable button kind " + std::to_string(kind) +
                 " answering no match");
        session->send(ShopPackets::rejectAscii("MSG_NO_MATCH_ROOM", 1));
        return;
    }

    // modes 0 1 cap at 8 modes 2 3 cap at 16 slot eight and up ran off the table
    const uint8_t cap = 8;

    std::shared_ptr<Room> target;
    bool fresh = false;   // a room opened for this player seats its partner after the human
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (auto& [id, r] : m_rooms) {
            if (!r || r->state() != RoomState::Waiting) continue;
            if (static_cast<int32_t>(r->settings().mode) != kind) continue;
            // a locked room asks a password on 0x2F so quick match never seats anyone there
            if (r->settings().isPrivate) continue;
            if (r->isFull()) continue;
            target = r;
            break;
        }
    }

    if (!target) {
        RoomSettings st;
        st.name = "Quick Match";
        st.mode = static_cast<GameMode>(kind);
        st.maxPlayers = cap;
        st.teamMode = (kind == 1 || kind == 3);
        // same rule as the explicit create path the room needs a track the 0xC3 catalogue can resolve
        st.mapId = static_cast<uint8_t>(defaultRoomTrack(0).mapId & 0xFF);
        st.laps = 3;
        st.fillWithBots = true;
        target = createRoom(st);
        fresh = target != nullptr;
    }
    if (!target) {
        LOG_WARN("GAME", "quick match could not open a room mode " + std::to_string(kind));
        session->send(ShopPackets::rejectAscii("MSG_NO_MATCH_ROOM", 1));
        return;
    }

    // the room stores the shown name nothing looks a player up by it
    if (!target->addPlayer(session, session->characterId, session->displayName(), 0)) {
        LOG_WARN("GAME", "quick match room " + std::to_string(target->id()) + " filled up first");
        session->send(ShopPackets::rejectAscii("MSG_NO_MATCH_ROOM", 1));
        return;
    }
    session->roomId = target->id();
    applyLoadout(target->getPlayer(session->id()), static_cast<int32_t>(session->characterId));

    // fillWithBots tops up the seat cap client refuses a room of one so a fallback adds one bot
    if (fresh && target->fillWithBotsIfEnabled() == 0) target->addBots(1);

    // the same chain a 0x2F join gets the 0x13 reaches stage 9 and its tail drops the MSG WAIT
    sendRoomScreen(session, target.get());
    announceRoomJoin(session, target.get());

    const RoomPlayer* rp = target->getPlayer(session->id());
    LOG_INFO("GAME", "quick match joined room " + std::to_string(target->id()) +
             " mode " + std::to_string(kind) + " slot " + std::to_string(rp ? rp->slot : 0));
}

// 0x39 is the host kick from sub 40D9D0 the client never sends it for a finish counted from checkpoints
void GameServer::handleRoomKick(Session::Ptr session, Packet& packet) {
    const int32_t targetId = packet.remaining() >= 4 ? packet.readInt32() : 0;
    auto room = getRoom(session->roomId);
    if (!room) return;
    if (!room->isHost(session->id())) {
        LOG_WARN("ROOM", "kick from char " + std::to_string(session->characterId) + " who is not host");
        return;
    }
    if (room->state() != RoomState::Waiting) return;
    if (targetId <= 0 || targetId == static_cast<int32_t>(session->characterId)) return;
    if (Room::isBotId(targetId)) {
        LOG_INFO("ROOM", "kick of bot " + std::to_string(targetId) + " ignored, bots leave with the room");
        return;
    }
    auto target = findSessionByCharacterId(targetId);
    if (!target || target->roomId != room->id()) {
        LOG_WARN("ROOM", "kick target " + std::to_string(targetId) + " is not in room " + std::to_string(room->id()));
        return;
    }
    LOG_INFO("ROOM", "host " + std::to_string(session->characterId) + " kicks " + std::to_string(targetId) +
             " from room " + std::to_string(room->id()));
    target->send(PacketBuilder::displayMessage(u"You were removed from the room by the host.", 1));
    Packet none;
    handleLeaveRoom(target, none);
}

void GameServer::handleGameStart(Session::Ptr session, Packet& packet) {
    (void)packet;
    auto room = getRoom(session->roomId);
    if (!room) {
        LOG_WARN("ROOM", "GameStart: player not in room");
        return;
    }
    
    if (!room->isHost(session->id())) {
        session->send(PacketBuilder::displayMessage(u"Only host can start", 0));
        LOG_WARN("ROOM", "Non-host tried to start game: char " + std::to_string(session->characterId));
        return;
    }
    
    if (!room->canStart()) {
        if (!room->areAllPlayersReady()) {
            session->send(PacketBuilder::displayMessage(u"Players not ready", 0));
        } else {
            session->send(PacketBuilder::displayMessage(u"Cannot start race now", 0));
        }
        LOG_WARN("ROOM", "Cannot start game: canStart=false");
        return;
    }
    
    LOG_INFO("ROOM", "Host " + std::to_string(session->characterId) + 
             " starting game in room " + std::to_string(room->id()));
    
    m_raceHandler.handleStartRace(session, room.get(), this);
}

void GameServer::handlePlayerReady(Session::Ptr session, Packet& packet) {
    bool ready = packet.readUInt8() != 0;

    auto room = getRoom(session->roomId);
    if (!room) return;

    room->setPlayerReady(session->id(), ready);

    auto* roomPlayer = room->getPlayer(session->id());
    if (!roomPlayer) return;

    PlayerData pd;
    pd.id = session->characterId;
    for (char16_t c : roomPlayer->name) {
        if (c > 0 && c < 128) pd.name += static_cast<char>(c);
    }
    pd.slot = roomPlayer->slot;
    pd.ready = ready;

    room->broadcast(PacketBuilder::playerUpdate(pd));

    LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) +
             " ready=" + std::to_string(ready) +
             " (ready count: " + std::to_string(room->readyCount()) + "/" +
             std::to_string(room->playerCount()) + ")");

    if (room->areAllPlayersReady() && room->playerCount() >= 2) {
        for (auto& sess : room->sessions()) {
            if (room->isHost(sess->id())) {
                sess->send(PacketBuilder::displayMessage(u"All players ready!", 1));
                break;
            }
        }
    }
}

// C2S 0x64 team change sub 4816A0 payload team 0 red 1 blue S2C reuses roomStatus builder sub 47ACD0
void GameServer::handleTeamChange(Session::Ptr session, Packet& packet) {
    int32_t team = packet.remaining() >= 4 ? packet.readInt32() : 0;

    // client sends 0 red 1 blue only
    if (team != 0 && team != 1) {
        LOG_WARN("ROOM", "Bad team " + std::to_string(team) + " from " + session->remoteAddress());
        return;
    }

    auto room = getRoom(session->roomId);
    if (!room) return;

    // team change only in team modes 1 and 3
    GameMode mode = room->settings().mode;
    if (mode != GameMode::ItemTeam && mode != GameMode::SpeedTeam) {
        session->send(ShopPackets::rejectAscii("MSG_TEAM_CHANGE_FAIL", 1));
        return;
    }

    // room stores 1 red 2 blue wire is 0 red 1 blue
    uint8_t stored = static_cast<uint8_t>(team + 1);
    int32_t cap = room->settings().maxPlayers;
    int32_t targetCount = 0;
    // bots hold a side too so count the seats the 0x21 drew not the humans alone
    for (const auto& p : room->participants()) {
        if (p.team == stored) ++targetCount;
    }

    if (targetCount >= cap / 2) {
        session->send(ShopPackets::rejectAscii("MSG_TEAM_CHANGE_FAIL", 1));
        return;
    }

    room->setPlayerTeam(session->id(), stored);

    // broadcast wire team so client seat flag matches
    room->broadcast(PacketBuilder::roomStatus(session->characterId, static_cast<uint8_t>(team)));

    LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) +
             " team=" + std::to_string(team));
}

// handleShopBrowse removed 0x68 is avatar pos not browse see SHOP CATALOG VERIFIED

void GameServer::handleSellItem(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleSellItem(session, packet, this);
}

void GameServer::handleEquipVehicle(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleEquipVehicle(session, packet, this);
}

void GameServer::handleEquipAccessory(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleEquipAccessory(session, packet, this);
}

void GameServer::handleUseItem(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleUseItem(session, packet, this);
}

void GameServer::sendRoomScreen(const std::shared_ptr<Session>& session, Room* room) {
    if (!session || !room) return;
    const RoomSettings& st = room->settings();
    const RoomTrackChoice track = defaultRoomTrack(st.mapId);

    RoomData rd;
    rd.id             = room->id();
    rd.name           = st.name;
    rd.mode           = static_cast<uint8_t>(st.mode);
    rd.maxPlayers     = st.maxPlayers;
    rd.mapId          = st.mapId;
    rd.laps           = st.laps;
    rd.currentPlayers = static_cast<uint8_t>(room->playerCount());
    // BCE220 is the room master the start button follows it so a joiner must get the real host
    rd.hostId         = room->hostCharacterId();
    rd.trackId        = track.trackId;

    std::vector<uint8_t> batch;
    auto append = [&batch](Packet p) {
        auto d = p.serialize();
        batch.insert(batch.end(), d.begin(), d.end());
    };
    // no 0x63 or 0x3F here sub 47AC30 opens the MakeRoom popup and sub 47A050 despawns a racer
    {
        Packet ctx = PacketBuilder::playerRoomData(rd, {});
        RoomCraftHandler::appendRoomDecor(ctx, static_cast<int32_t>(rd.id), room->hostCharacterId());
        append(std::move(ctx));
    }
    // enable seats the mode allows and disable the rest or unused slots keep stale state and draw as ghost seats
    for (uint32_t i = 0; i < 30u; ++i) append(PacketBuilder::roomSlotEnabled(i, i < rd.maxPlayers));
    // every other human first the receiver last then the CPU cars
    for (const auto& p : room->getPlayers()) {
        if (p.sessionId == session->id()) continue;
        append(PacketBuilder::roomMember(buildRoomMember(p.characterId, p.name, p.slot, p.team, p.ready)));
    }
    if (const RoomPlayer* self = room->getPlayer(session->id())) {
        append(PacketBuilder::roomMember(buildRoomMember(
            self->characterId, self->name, self->slot, self->team, self->ready)));
    }
    for (const auto& b : room->bots()) append(PacketBuilder::roomMember(buildBotMember(b)));

    // 0x30 lands in BCE220 too so it carries the room master and clears that one ready flag
    append(PacketBuilder::roomState(room->hostCharacterId()));
    // 0x35 the room track without it the client freezes on its first 0x40 the id must resolve in 0xC3
    append(PacketBuilder::roomTrackSelect(track.trackId));
    // client recv buffer is a fixed 8192 never let one write cross it
    session->send(batch);
}

void GameServer::announceRoomJoin(const std::shared_ptr<Session>& session, Room* room) {
    if (!session || !room) return;
    const RoomPlayer* self = room->getPlayer(session->id());
    if (!self) return;
    // 0x32 before 0x21 or sub 40D650 bails on the disabled slot guard
    room->broadcastExcept(PacketBuilder::roomSlotEnabled(self->slot, true), session->id());
    room->broadcastExcept(PacketBuilder::roomMember(buildRoomMember(
        self->characterId, self->name, self->slot, self->team, self->ready)), session->id());
}

void GameServer::handleRequestData(Session::Ptr session, Packet& packet) {
    // client pushes a static 276 byte blob from unk 5DE9DC no reply expected
    if (packet.remaining() < 276) {
        LOG_WARN("GAME", "CLIENT_INFO (0x4D) too small from " + session->remoteAddress() +
                 " (" + std::to_string(packet.remaining()) + " bytes)");
        return;
    }
    uint32_t requestType = packet.readUInt32();
    (void)packet.readBytes(272);
    LOG_DEBUG("GAME", "CLIENT_INFO (0x4D) type=" + std::to_string(requestType) +
              " from " + session->remoteAddress());
}

void GameServer::handleUnknown32(Session::Ptr session, Packet& packet) {
    if (packet.remaining() < 8) {
        return;
    }

    uint32_t data1 = packet.readUInt32();
    uint32_t data2 = packet.readUInt32();

    LOG_DEBUG("GAME", "CMD 0x32: " + std::to_string(data1) + ", " + std::to_string(data2) +
              " from " + session->remoteAddress());
}

}  // namespace knc
