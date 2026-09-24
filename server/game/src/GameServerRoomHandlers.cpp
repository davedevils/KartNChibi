/// auth handlers lobby handlers room handlers and the room member wire builders they share

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
#include "packets/gen/CharCreatePackets.h"
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

void GameServer::handleHeartbeat(Session::Ptr session, Packet& packet) {
    (void)packet;
    // do not respond 0x12 would trigger a lobby transition but still prove the player is alive to the reaper
    if (session->accountId == 0) return;
    static thread_local std::unordered_map<uint32_t, uint64_t> lastTouch;
    const uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    auto& t = lastTouch[session->id()];
    if (now - t < 60) return;   // heartbeat is 1 Hz do not hammer the db
    t = now;
    Database::instance().executePrepared(
        "UPDATE online_players SET last_seen = NOW() WHERE account_id = ? AND game_session = ?",
        {static_cast<int32_t>(session->accountId), static_cast<int32_t>(session->id())});
}

void GameServer::handleClientAuth(Session::Ptr session, Packet& packet) {
    // 0x07 reaches the game server only from a client that skipped the login server same body as the login 0x07
    std::string version;
    int32_t action = 0;
    std::string username, password;
    if (packet.remaining() >= 4) version = packet.readString(4);
    if (packet.remaining() >= 4) action = packet.readInt32();
    if (packet.remaining() >= 4) {
        const std::u16string u = packet.readWString(32);
        const std::u16string pw = packet.readWString(32);
        for (char16_t c : u)  { if (c == 0) break; username += c < 128 ? static_cast<char>(c) : '?'; }
        for (char16_t c : pw) { if (c == 0) break; password += c < 128 ? static_cast<char>(c) : '?'; }
    }
    LOG_INFO("GAME", "Client auth 0x07 from " + session->remoteAddress() + " user='" + username +
             "' action=" + std::to_string(action));

    auto reject = [&]() {
        Packet err(CMD::S_LOGIN_RESPONSE);
        err.writeString("MSG_REINPUT_IDPASS");
        err.writeInt32(1);
        session->send(err);
        session->closeAfterSend();
    };
    if (username.empty() || password.empty()) { reject(); return; }

    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT id, password_hash, is_banned FROM accounts WHERE username = ?", {username});
    if (rows.empty() || rows[0]["is_banned"] == "1") { reject(); return; }
    bool ok = false;
    try { ok = PasswordHash::verify(password, rows[0]["password_hash"]); } catch (...) { ok = false; }
    if (!ok) {
        LOG_WARN("GAME", "0x07 password refused for user '" + username + "'");
        reject();
        return;
    }
    const uint32_t accountId = static_cast<uint32_t>(std::stoul(rows[0]["id"]));
    uint32_t characterId = 0;
    auto chars = db.queryPrepared(
        "SELECT id FROM characters WHERE account_id = ? ORDER BY id LIMIT 1",
        {static_cast<int32_t>(accountId)});
    if (!chars.empty()) characterId = static_cast<uint32_t>(std::stoul(chars[0]["id"]));

    // one account one socket the same rule the login server applies
    auto live = db.queryPrepared(
        "SELECT game_session FROM online_players WHERE account_id = ? "
        "AND last_seen > NOW() - INTERVAL 90 SECOND",
        {static_cast<int32_t>(accountId)});
    if (!live.empty() && getSession(static_cast<uint32_t>(std::stoul(live[0]["game_session"])))) {
        LOG_WARN("GAME", "0x07 refused, account " + std::to_string(accountId) + " is already in game");
        Packet msg(CMD::S_DISPLAY_MESSAGE);
        msg.writeWString(u"That account is already logged in.");
        msg.writeInt32(2);
        session->send(msg);
        session->closeAfterSend();
        return;
    }
    // any redirect ticket the login server left is spent
    db.executePrepared("DELETE FROM active_sessions WHERE account_id = ?", {static_cast<int32_t>(accountId)});
    bindAccount(session, accountId, characterId);
    sendPlayerData(session);
}

void GameServer::handleFullState(Session::Ptr session, Packet& packet) {
    (void)packet;
    session->send(PacketBuilder::initResponse());
}

void GameServer::handleClientInfo(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_INFO("GAME", "Client info from " + session->remoteAddress());
    session->send(PacketBuilder::ack());
}

void GameServer::handleSessionConfirm(Session::Ptr session, Packet& packet) {
    // C2S 0xA7 sub 480500 carries the redirect ticket the login server put in the profile blob
    std::string version;
    int32_t stage = 0;
    std::string token;
    uint32_t accountId = 0;
    if (packet.remaining() >= 5) version = packet.readString(4);
    if (packet.remaining() >= 4) stage = packet.readInt32();
    if (packet.remaining() >= 2) {
        const std::u16string w = packet.readWString(64);
        for (char16_t c : w) { if (c == 0) break; token += (c < 128) ? static_cast<char>(c) : '?'; }
    }
    if (packet.remaining() >= 4) accountId = packet.readUInt32();
    LOG_INFO("GAME", "Session confirm (0xA7) from " + session->remoteAddress() + " version=" + version +
             " stage=" + std::to_string(stage) + " account=" + std::to_string(accountId) +
             " token=" + (token.size() > 8 ? token.substr(0, 8) + "..." : token));

    auto reject = [&](const char* why) {
        LOG_WARN("GAME", std::string("0xA7 refused, ") + why + " from " + session->remoteAddress());
        Packet msg(CMD::S_DISPLAY_MESSAGE);
        msg.writeWString(u"Your login expired, please log in again.");
        msg.writeInt32(2);
        session->send(msg);
        session->closeAfterSend();
    };
    if (token.empty() || accountId == 0) { reject("no ticket in the frame"); return; }

    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT account_id, character_id FROM active_sessions WHERE token = ? AND account_id = ? "
        "AND created_at > NOW() - INTERVAL 5 MINUTE ORDER BY created_at DESC LIMIT 1",
        {token, static_cast<int32_t>(accountId)});
    if (rows.empty()) { reject("no matching redirect ticket"); return; }
    const uint32_t characterId = static_cast<uint32_t>(std::stoul(rows[0]["character_id"]));
    db.executePrepared("DELETE FROM active_sessions WHERE account_id = ?", {static_cast<int32_t>(accountId)});
    session->sessionToken = token;
    bindAccount(session, accountId, characterId);
    sendPlayerData(session);
}

void GameServer::bindAccount(Session::Ptr session, uint32_t accountId, uint32_t characterId) {
    // an older socket on the same account is a zombie dropped before this one takes the row
    for (const auto& other : getSessions()) {
        if (!other || other == session || other->accountId != accountId) continue;
        LOG_WARN("GAME", "account " + std::to_string(accountId) + " rebinds from session " +
                 std::to_string(other->id()) + " to " + std::to_string(session->id()) +
                 ", the old socket is dropped");
        other->accountId = 0;   // its disconnect must not erase the new row
        other->stop();
    }
    session->accountId = accountId;
    session->characterId = characterId;
    Database::instance().executePrepared(
        "INSERT INTO online_players (account_id, character_id, game_session, since, last_seen) "
        "VALUES (?, ?, ?, NOW(), NOW()) "
        "ON DUPLICATE KEY UPDATE character_id = VALUES(character_id), "
        "game_session = VALUES(game_session), since = NOW(), last_seen = NOW()",
        {static_cast<int32_t>(accountId), static_cast<int32_t>(characterId),
         static_cast<int32_t>(session->id())});
    LOG_INFO("GAME", "account " + std::to_string(accountId) + " char " + std::to_string(characterId) +
             " bound to session " + std::to_string(session->id()));
}

void GameServer::handleChannelSelect(Session::Ptr session, Packet& packet) {
    int32_t screen = packet.readInt32();
    int32_t channelId = packet.readInt32();

    LOG_INFO("GAME", "Channel select from " + session->remoteAddress() +
             " screen=0x" + toHex(screen) + " channel=" + std::to_string(channelId));

    // the answer follows the screen the client asks for 8 gets the main menu 0x0E gets the lobby
    if (screen == 0x0E) {
        Packet showLobby(CMD::S_SHOW_LOBBY);
        session->send(showLobby);
        sendLobbyRoomList(session);
    } else {
        Packet showMenu(CMD::S_SHOW_MENU);
        session->send(showMenu);
    }
}

void GameServer::broadcastLobbyRoomRemove(uint32_t roomId) {
    auto pkt = PacketBuilder::lobbyRoomRemove(roomId);
    for (const auto& s : getLobbySessions()) {
        if (s) s->send(pkt);
    }
}

void GameServer::leaveCurrentRoom(const std::shared_ptr<Session>& session) {
    if (!session || session->roomId == 0) return;
    auto room = getRoom(session->roomId);
    const uint32_t roomId = session->roomId;
    session->roomId = 0;
    if (!room) return;

    const bool wasHost = room->isHost(session->id());
    const uint32_t oldHost = room->hostSessionId();
    room->removePlayer(session->id());

    // isEmpty looks at humans only a grid of bots is still a dead room
    if (room->isEmpty()) {
        LOG_INFO("ROOM", "room " + std::to_string(roomId) + " lost its last human, removing");
        removeRoom(roomId);
        broadcastLobbyRoomRemove(roomId);
        return;
    }

    room->broadcast(PacketBuilder::playerLeft(static_cast<int32_t>(session->characterId)));
    if (wasHost && room->hostSessionId() != oldHost) {
        auto* newHost = room->getPlayer(room->hostSessionId());
        if (newHost) {
            PlayerData hostData;
            hostData.id = newHost->characterId;
            for (char16_t c : newHost->name) {
                if (c > 0 && c < 128) hostData.name += static_cast<char>(c);
            }
            hostData.slot = newHost->slot;
            room->broadcast(PacketBuilder::playerUpdate(hostData));
        }
    }
    // the row counter moved so everyone in the lobby needs the new numbers
    for (const auto& s : getLobbySessions()) sendLobbyRoomList(s);
}

void GameServer::sendLobbyRoomList(const std::shared_ptr<Session>& session) {
    if (!session) return;
    // grid cleared by sub 408020 on every entry so the whole list must resend one 0x2D per room
    std::vector<Packet> burst;
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (const auto& [id, room] : m_rooms) {
            if (!room) continue;
            const auto& st = room->settings();
            std::string n = room->name();
            std::u16string wname(n.begin(), n.end());
            // icon must land in 0 to 4 or sub 407EA0 drops the row
            int32_t icon = static_cast<int32_t>(st.mode);
            if (icon < 0 || icon > 4) icon = 0;
            // sub 408360 takes the first free slot and never checks the id so drop duplicates before appending
            burst.push_back(PacketBuilder::lobbyRoomRemove(id));
            burst.push_back(PacketBuilder::lobbyRoomAdd(
                id, wname,
                static_cast<int32_t>(room->humanCount()),
                static_cast<int32_t>(st.maxPlayers),
                icon,
                st.isPrivate ? 1 : 0,
                static_cast<int32_t>(room->state()),
                0));
        }
    }
    LOG_INFO("LOBBY", "room list to " + session->remoteAddress() + " rows " +
             std::to_string(burst.size()));
    if (burst.empty()) return;
    sendDripped(session, std::move(burst));
}

void GameServer::handleLobbyRequest(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_INFO("GAME", "Lobby request from " + session->remoteAddress());

    std::vector<RoomData> roomList;
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (const auto& [id, room] : m_rooms) {
            RoomData rd;
            rd.id = id;
            rd.name = room->name();
            rd.currentPlayers = static_cast<uint8_t>(room->playerCount());
            rd.maxPlayers = room->settings().maxPlayers;
            rd.mode = static_cast<uint8_t>(room->settings().mode);
            rd.mapId = room->settings().mapId;
            rd.state = static_cast<uint8_t>(room->state());
            rd.isPrivate = room->settings().isPrivate;
            roomList.push_back(rd);
        }
    }

    session->send(PacketBuilder::showLobby(roomList));
    sendLobbyRoomList(session);
}

// C2S 0x0019 the channel return sub 4817E0 sends the ini host the ini port and mode 4
void GameServer::handleServerQuery(Session::Ptr session, Packet& packet) {
    const std::string askedHost = packet.readString(128);
    const uint32_t askedPort = packet.remaining() >= 4 ? packet.readUInt32() : 0;
    const uint32_t mode = packet.remaining() >= 4 ? packet.readUInt32() : 4;

    // FUN 00405EF0 disconnects reconnects mode 4 uses ini port then sends C2S 0x00A7 with mode as stage
    const std::string host = m_loginHost.empty() ? askedHost : m_loginHost;
    const uint32_t port = m_loginPort != 0 ? static_cast<uint32_t>(m_loginPort) : askedPort;

    LOG_INFO("GAME", "channel return from char " + std::to_string(session->characterId) +
             " redirect to " + host + ":" + std::to_string(port) +
             " mode " + std::to_string(mode));
    session->send(CharCreatePackets::serverRedirect(host, port, mode));
}

// C CREATE ROOM REQ 0x2D wire format from sub 480CC0 the 0x21 member must match sub 40D650 lookup
RoomMemberWire buildBotMember(const RoomPlayer& bot) {
    RoomMemberWire m;
    m.slot = bot.slot;
    m.team = wireTeam(bot.team);
    m.playerId = static_cast<uint32_t>(bot.characterId);
    m.displayName.assign(bot.name.begin(), bot.name.end());
    m.levelIndex = 0;
    m.readyState = 1;   // a bot is always ready or the host can never start

    auto& db = Database::instance();

    InventoryPackets::CharacterRow cr;
    cr.instanceId = static_cast<uint32_t>(bot.characterId);
    cr.baseKey    = static_cast<uint32_t>(bot.driverId);
    cr.activeFlag = 1;
    {
        auto asset = db.queryPrepared("SELECT name FROM drivers WHERE id = ? LIMIT 1",
                                      {bot.driverId});
        if (!asset.empty()) {
            const std::string a = driverBodyAsset(asset[0].at("name"));
            auto sk = db.queryPrepared(
                "SELECT skin_key, name FROM def_kart_skin WHERE name LIKE ? "
                "ORDER BY skin_key LIMIT 16", {a + "\_char\_%"});
            // first match wins the rows come lowest key first and the lowest is the default costume a real character wears
            for (const auto& r : sk) {
                const std::string& n = r.at("name");
                const int32_t k = std::stoi(r.at("skin_key"));
                if (cr.accBody == 0 && n.find("_char_body_") != std::string::npos) cr.accBody = k;
                if (cr.accFace == 0 && n.find("_char_face_") != std::string::npos) cr.accFace = k;
                if (cr.accHead == 0 && n.find("_char_head_") != std::string::npos) cr.accHead = k;
            }
        }
    }
    m.character = InventoryPackets::characterBlob(cr);

    InventoryPackets::KartRow kr;
    kr.instanceId    = static_cast<uint32_t>(bot.characterId);
    kr.baseKey       = static_cast<uint32_t>(bot.vehicleTemplateId);
    kr.skinPrimary   = static_cast<uint32_t>(kDefaultPaintKey);
    kr.skinSecondary = static_cast<uint32_t>(kDefaultPlateKey);
    kr.periodMode    = 3;
    kr.periodValue   = 100;
    kr.activeFlag    = 1;
    m.kart = InventoryPackets::kartBlob(kr);

    return m;
}

RoomMemberWire buildRoomMember(int32_t charId, const std::u16string& name,
                                      uint32_t slot, uint8_t team, bool ready) {
    RoomMemberWire m;
    m.slot = slot;
    // sub 40D650 counts 0 as red and 1 as blue the room side is converted here
    m.team = wireTeam(team);
    m.playerId = static_cast<uint32_t>(charId);
    m.displayName = name;
    m.levelIndex = 0;
    m.readyState = ready ? 1u : 0u;

    auto& db = Database::instance();

    int32_t driverId = 0;
    auto drv = db.queryPrepared(
        // driver id never existed real columns are driver base key and equipped driver id sub 40D650 needs both
        "SELECT COALESCE(NULLIF(driver_base_key,0), equipped_driver_id, 0) AS driver_id, "
        "COALESCE(pendant_key, 0) AS pendant_key "
        "FROM characters WHERE id = ? LIMIT 1",
        {charId});
    if (!drv.empty()) driverId = std::stoi(drv[0].at("driver_id"));
    // sub 499180 draws Icon base s left of the room name plate from this u32 the worn pendant
    if (!drv.empty()) m.titleKey = static_cast<uint32_t>(std::stoul(drv[0].at("pendant_key")));
    if (driverId == 0) {
        auto anyDrv = db.queryPrepared(
            "SELECT id FROM drivers WHERE COALESCE(is_enabled, 1) = 1 ORDER BY id LIMIT 1", {});
        if (!anyDrv.empty()) driverId = std::stoi(anyDrv[0].at("id"));
    }
    // the five cosmetic slots the client hangs on O BODY O FACE O HEAD O GLASS O BACK
    std::array<int32_t, 5> charSlots{};
    {
        auto asset = db.queryPrepared("SELECT name FROM drivers WHERE id = ? LIMIT 1", {driverId});
        if (!asset.empty()) {
            const std::string a = driverBodyAsset(asset[0].at("name"));
            auto sk = db.queryPrepared(
                "SELECT skin_key, name FROM def_kart_skin WHERE category = 2 "
                "AND name LIKE ? ORDER BY skin_key LIMIT 16", {a + "\_char\_%"});
            for (const auto& r : sk) {
                const std::string& n = r.at("name");
                const int32_t k = std::stoi(r.at("skin_key"));
                if (n.find("_char_body_") != std::string::npos) charSlots[0] = k;
                if (n.find("_char_face_") != std::string::npos) charSlots[1] = k;
                if (n.find("_char_head_") != std::string::npos) charSlots[2] = k;
            }
        }
    }
    // build from the same owned row the 0x1B list published not a rebuilt record
    {
        InventoryPackets::CharacterRow cr;
        if (InventoryHandler::selectedCharacterRow(charId, cr)) {
            m.character = InventoryPackets::characterBlob(cr);
        } else {
            LOG_ERROR("GAME", "char " + std::to_string(charId) +
                      " has no owned_character row so the room member falls back to the "
                      "driver table and the equipped accessories are lost");
            m.character = PacketBuilder::characterRecord(driverId, driverId, charSlots);
        }
    }

    // same rule for the kart sub 40D650 resolves kartRec 0x04 and 0x08 a missing colour key fails Set body
    {
        InventoryPackets::KartRow kr;
        if (InventoryHandler::selectedKartRow(charId, kr)) {
            m.kart = InventoryPackets::kartBlob(kr);
            m.customCar = CustomCarPackets::customCarBlockFor(charId, kr.instanceId);
            {
                // sub 40d650 copies member kart 0x08 over catalog 0x84 and sub 490a70 bails when that key misses
                std::string hex;
                for (size_t i = 0; i < 0x14; ++i) {
                    char b[4];
                    std::snprintf(b, sizeof(b), "%02X ", m.kart[i]);
                    hex += b;
                }
                LOG_INFO("GAME", "member kart char " + std::to_string(charId) +
                         " inst " + std::to_string(kr.instanceId) +
                         " base " + std::to_string(kr.baseKey) +
                         " paint " + std::to_string(kr.skinPrimary) +
                         " head20 " + hex);
            }
        } else {
            LOG_ERROR("GAME", "char " + std::to_string(charId) +
                      " has no owned_kart row, the room member would carry an all zero "
                      "kart which the client drops without a word, sending nothing");
        }
    }

    return m;
}

void GameServer::handleCreateRoom(Session::Ptr session, Packet& packet) {
    std::u16string roomNameW = packet.readWString();
    std::u16string passwordW = packet.readWString();

    std::string roomName, password;
    for (char16_t c : roomNameW) if (c > 0 && c < 128) roomName += static_cast<char>(c);
    for (char16_t c : passwordW) if (c > 0 && c < 128) password += static_cast<char>(c);

    // client field order maxUsers before mode then flag then public 0 public 1 private
    int32_t maxUsers    = packet.remaining() >= 4 ? packet.readInt32() : 8;
    int32_t mode        = packet.remaining() >= 4 ? packet.readInt32() : 0;
    int32_t flag        = packet.remaining() >= 4 ? packet.readInt32() : 0;
    int32_t privacyFlag = packet.remaining() >= 4 ? packet.readInt32() : 0;
    // mapId and laps arrive later via 0x14 but the room needs a track now so pick the first real one
    const RoomTrackChoice pick = defaultRoomTrack(0);
    int32_t mapId = pick.mapId;
    int32_t laps  = 3;

    LOG_INFO("ROOM", "Create room: '" + roomName + "' pwd='" + password +
             "' maxUsers=" + std::to_string(maxUsers) + " mode=" + std::to_string(mode) +
             " flag=" + std::to_string(flag) + " public=" + std::to_string(privacyFlag));

    RoomSettings settings;
    settings.name = roomName;
    settings.password = password;
    settings.mode = static_cast<GameMode>(mode);
    // speed modes hold sixteen item and battle hold eight per client sub 4634C0
    int32_t modeCap = (mode == 2 || mode == 3) ? 16 : 8;
    settings.maxPlayers = static_cast<uint8_t>(maxUsers > 0 && maxUsers <= modeCap ? maxUsers : modeCap);
    settings.mapId = static_cast<uint8_t>(mapId & 0xFF);
    settings.laps = static_cast<uint8_t>(laps > 0 ? laps : 3);
    settings.isPrivate = !password.empty() || privacyFlag != 0;
    // team modes 1 and 3 need team logic see ROOM MISSION OPCODES VERIFIED md
    settings.teamMode = (mode == 1 || mode == 3);

    auto room = createRoom(settings);
    if (!room) {
        // 0x63 opens the MakeRoom popup with a mode index so a refusal is a text box not a 0x63
        session->send(PacketBuilder::displayMessage(u"Could not create the room", 0));
        LOG_WARN("ROOM", "Failed to create room");
        return;
    }

    // the room stores the shown name nothing looks a player up by it
    if (!room->addPlayer(session, session->characterId, session->displayName(), 0)) {
        LOG_ERROR("ROOM", "fresh room " + std::to_string(room->id()) + " refused its creator");
        session->send(PacketBuilder::displayMessage(u"Could not create the room", 0));
        removeRoom(room->id());
        return;
    }
    session->roomId = room->id();
    applyLoadout(room->getPlayer(session->id()), static_cast<int32_t>(session->characterId));
    room->fillWithBotsIfEnabled();   // off by default here honoured if a caller ever turns it on

    // 0x63 and 0x3F were never a create ack the creator gets the same chain a joiner gets
    sendRoomScreen(session, room.get());
    LOG_INFO("ROOM", "Room created ID=" + std::to_string(room->id()) + " '" + roomName + "'");
}

void GameServer::handleJoinRoom(Session::Ptr session, Packet& packet) {
    uint32_t roomId = packet.readUInt32();
    // 0x2F writes room id then password as a NUL terminated wstr via sub 480F30 ascii reading broke locked rooms
    std::string password;
    if (packet.remaining() >= 2) {
        const std::u16string wide = packet.readWString(16);
        for (char16_t c : wide) password.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    }
    
    auto room = getRoom(roomId);

    if (!room) {
        LOG_WARN("ROOM", "Room not found: " + std::to_string(roomId));
        session->send(PacketBuilder::displayMessage(u"Room not found", 0));
        return;
    }

    if (room->state() != RoomState::Waiting) {
        session->send(PacketBuilder::displayMessage(u"Game in progress", 0));
        return;
    }

    if (room->isFull()) {
        // 0x21 not room full use displayMessage
        session->send(PacketBuilder::displayMessage(u"Room is full", 0));
        return;
    }

    if (room->settings().isPrivate && room->settings().password != password) {
        session->send(PacketBuilder::displayMessage(u"Wrong password", 0));
        return;
    }

    if (session->roomId != 0) {
        auto oldRoom = getRoom(session->roomId);
        if (oldRoom) {
            oldRoom->removePlayer(session->id());
        }
    }

    // refuse if no owned kart never fake template
    auto& db = Database::instance();
    int32_t vehicleTemplateId = 0;
    auto vehResult = db.queryPrepared(
        "SELECT k.base_key FROM owned_kart k JOIN characters c ON c.id = k.character_id "
        "WHERE k.character_id = ? AND c.selected_kart_instance_id = k.id LIMIT 1",
        {std::to_string(session->characterId)}
    );
    if (!vehResult.empty()) {
        vehicleTemplateId = std::stoi(vehResult[0]["base_key"]);
    } else {
        // the selection can name a deleted row so any owned kart still gets the player in
        auto anyVeh = db.queryPrepared(
            "SELECT base_key FROM owned_kart WHERE character_id = ? "
            "ORDER BY active_flag DESC, id ASC LIMIT 1",
            {std::to_string(session->characterId)}
        );
        if (!anyVeh.empty()) {
            vehicleTemplateId = std::stoi(anyVeh[0]["base_key"]);
        }
    }
    if (vehicleTemplateId == 0) {
        LOG_WARN("ROOM", "Join refused: character " + std::to_string(session->characterId) +
                 " has no owned kart row");
        session->send(PacketBuilder::displayMessage(u"No vehicle equipped", 0));
        return;
    }
    
    // the room stores the shown name nothing looks a player up by it
    if (room->addPlayer(session, session->characterId, session->displayName(), vehicleTemplateId)) {
        session->roomId = room->id();
        applyLoadout(room->getPlayer(session->id()), static_cast<int32_t>(session->characterId));

        // join snapshot 0x13 context then the 0x32 slots then every member joiner last then 0x30 and 0x35
        sendRoomScreen(session, room.get());
        // peers learn the joiner the same way slot first then member
        announceRoomJoin(session, room.get());

        LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) +
                 " joined room " + std::to_string(roomId) +
                 " (total: " + std::to_string(room->playerCount()) + ")");
    } else {
        // the seat went between the isFull check and here never leave the joiner on a dead screen
        LOG_WARN("ROOM", "no free seat in room " + std::to_string(roomId) +
                 " for char " + std::to_string(session->characterId));
        session->send(PacketBuilder::displayMessage(u"Room is full", 0));
    }
}

void GameServer::handleLeaveRoom(Session::Ptr session, Packet& packet) {
    (void)packet;
    
    auto room = getRoom(session->roomId);
    if (room) {
        bool wasHost = room->isHost(session->id());
        uint32_t oldHostSession = room->hostSessionId();
        
        room->removePlayer(session->id());
        
        if (!room->isEmpty()) {
            room->broadcast(PacketBuilder::playerLeft(session->characterId));

            if (wasHost && room->hostSessionId() != oldHostSession) {
                auto* newHost = room->getPlayer(room->hostSessionId());
                if (newHost) {
                    PlayerData hostData;
                    hostData.id = newHost->characterId;
                    for (char16_t c : newHost->name) {
                        if (c > 0 && c < 128) hostData.name += static_cast<char>(c);
                    }
                    hostData.slot = newHost->slot;
                    room->broadcast(PacketBuilder::playerUpdate(hostData));
                    LOG_INFO("ROOM", "Host transferred to char " + std::to_string(newHost->characterId));
                }
            }
        } else {
            LOG_INFO("ROOM", "Room " + std::to_string(room->id()) + " is now empty, removing");
            removeRoom(room->id());
        }
    }
    
    session->roomId = 0;
    session->send(PacketBuilder::playerDisconnect(session->characterId));
    session->send(PacketBuilder::showLobby({}));
    
    LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) + " left room");
}

}  // namespace knc
