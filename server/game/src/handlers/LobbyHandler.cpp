/// handles lobby packets certified from IDA analysis

#include "handlers/LobbyHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"

namespace knc {

void LobbyHandler::handleRoomListRequest(Session::Ptr session, GameServer* server) {
    LOG_DEBUG("LOBBY", "Room list request from " + session->remoteAddress());
    sendRoomList(session, server);
}

void LobbyHandler::handleQuickMatch(Session::Ptr session, Packet& packet, GameServer* server) {
    // C2S 0x64 outside a room is the quick match of our own client the stock one sends 0x2C
    const int32_t mode = packet.remaining() >= 4 ? packet.readInt32() : 0;
    LOG_INFO("LOBBY", "Quick match request on 0x64 mode " + std::to_string(mode));
    // same seat and the same 0x13 chain a 0x2F join gets 0x63 and 0x3F never opened a room
    server->quickMatch(session, mode);
}

void LobbyHandler::handlePlayerListRequest(Session::Ptr session, GameServer* server) {
    LOG_DEBUG("LOBBY", "Player list request from " + session->remoteAddress());
    sendPlayerList(session, server);
}

void LobbyHandler::handlePlayerProfile(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    
    int32_t playerId = packet.readInt32();
    LOG_DEBUG("LOBBY", "Profile request: playerId=" + std::to_string(playerId));
    sendPlayerProfile(session, playerId);
}

void LobbyHandler::handleLobbyChat(Session::Ptr session, Packet& packet, GameServer* server) {
    std::u16string message = packet.readWString();

    std::string senderName;
    for (char16_t c : session->characterName) {
        if (c > 0 && c < 128) senderName += static_cast<char>(c);
    }
    
    LOG_DEBUG("LOBBY", "Chat from " + senderName);
    
    // broadcast to all players in lobby not in rooms
    auto lobbySessions = server->getLobbySessions();
    for (const auto& s : lobbySessions) {
        s->send(PacketBuilder::lobbyChatBroadcast(
            static_cast<int32_t>(session->characterId), session->characterName, message, 0));
    }
}

void LobbyHandler::handleWhisper(Session::Ptr session, Packet& packet, GameServer* server) {
    std::u16string targetName = packet.readWString();
    std::u16string message = packet.readWString();
    
    std::string senderStr;
    for (char16_t c : session->characterName) {
        if (c > 0 && c < 128) senderStr += static_cast<char>(c);
    }

    std::string targetStr;
    for (char16_t c : targetName) {
        if (c > 0 && c < 128) targetStr += static_cast<char>(c);
    }
    
    LOG_DEBUG("LOBBY", "Whisper from " + senderStr + " to " + targetStr);
    
    Session::Ptr targetSession = nullptr;
    for (auto& sess : server->getLobbySessions()) {
        if (sess && sess->characterName == targetName) {
            targetSession = sess;
            break;
        }
    }
    
    if (targetSession) {
        targetSession->send(PacketBuilder::lobbyChatBroadcast(
            static_cast<int32_t>(session->characterId), session->characterName, message, 1));
        session->send(PacketBuilder::whisperEnable());
        LOG_INFO("LOBBY", "Whisper sent to " + targetStr);
    } else {
        session->send(PacketBuilder::systemMessage(u"Player not found.", 0));
        LOG_WARN("LOBBY", "Whisper target not found: " + targetStr);
    }
}

void LobbyHandler::handleAddFriend(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    
    std::u16string friendName = packet.readWString();
    
    std::string friendNameNarrow;
    for (char16_t c : friendName) {
        if (c > 0 && c < 128) friendNameNarrow += static_cast<char>(c);
    }
    
    auto& db = Database::instance();
    
    auto friends = db.queryPrepared(
        "SELECT id FROM characters WHERE name = ?", {friendNameNarrow});
    
    if (friends.empty()) {
        session->send(PacketBuilder::displayMessage(u"Player not found.", 2));
        return;
    }
    
    int32_t friendId = std::stoi(friends[0]["id"]);
    
    db.executePrepared(
        "INSERT IGNORE INTO friends (character_id, friend_id) VALUES (?, ?)",
        {std::to_string(session->characterId), std::to_string(friendId)});
    
    session->send(PacketBuilder::displayMessage(u"Friend added.", 1));
}

void LobbyHandler::handleRemoveFriend(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    
    int32_t friendId = packet.readInt32();
    
    auto& db = Database::instance();
    
    db.executePrepared(
        "DELETE FROM friends WHERE character_id = ? AND friend_id = ?",
        {std::to_string(session->characterId), std::to_string(friendId)});
    
    session->send(PacketBuilder::displayMessage(u"Friend removed.", 1));
}

void LobbyHandler::handleBlockPlayer(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    
    int32_t playerId = packet.readInt32();
    
    auto& db = Database::instance();
    
    db.executePrepared(
        "INSERT IGNORE INTO blocked_players (character_id, blocked_id) VALUES (?, ?)",
        {std::to_string(session->characterId), std::to_string(playerId)});
    
    session->send(PacketBuilder::displayMessage(u"Player blocked.", 1));
}

void LobbyHandler::sendRoomList(Session::Ptr session, GameServer* server) {
    auto roomList = getRoomList(server);
    
    LOG_DEBUG("LOBBY", "Sending room list: " + std::to_string(roomList.size()) + " rooms");
    
    std::vector<RoomData> rooms;
    for (const auto& r : roomList) {
        RoomData rd;
        rd.id = r.id;
        for (size_t i = 0; i < r.name.size() && i < 31; i++) {
            rd.name[i] = static_cast<char16_t>(r.name[i]);
        }
        rd.mode = r.mode;
        rd.maxPlayers = r.maxPlayers;
        rd.mapId = r.mapId;
        rooms.push_back(rd);
    }
    
    session->send(PacketBuilder::showLobby(rooms));
}

void LobbyHandler::sendPlayerList(Session::Ptr session, GameServer* server) {
    auto players = getPlayerList(server);
    
    LOG_DEBUG("LOBBY", "Sending player list: " + std::to_string(players.size()) + " players");
    
    for (const auto& player : players) {
        if (player.id != session->id()) {
            PlayerData pdata;
            pdata.id = player.id;
            for (size_t i = 0; i < player.name.size() && i < 31; i++) {
                pdata.name[i] = player.name[i];
            }
            pdata.slot = 0;
            pdata.team = 0;
            pdata.ready = false;
            pdata.vehicleId = 0;
            session->send(PacketBuilder::broadcastPlayerData(pdata));
        }
    }
}

void LobbyHandler::sendPlayerProfile(Session::Ptr session, int32_t playerId) {
    auto& db = Database::instance();
    
    auto chars = db.queryPrepared(
        "SELECT c.id, c.name, c.level, c.experience, c.gold, c.wins, c.losses, "
        "c.total_races, c.playtime_minutes, c.license_class, c.rank_points "
        "FROM characters c WHERE c.id = ?",
        {std::to_string(playerId)});
    
    if (chars.empty()) {
        session->send(PacketBuilder::displayMessage(u"Player not found.", 2));
        return;
    }
    
    const auto& player = chars[0];
    
    std::u16string profileMsg;
    for (char c : player.at("name")) {
        profileMsg += static_cast<char16_t>(c);
    }
    profileMsg += u" - Lv.";
    for (char c : player.at("level")) {
        profileMsg += static_cast<char16_t>(c);
    }
    profileMsg += u" W:";
    for (char c : player.at("wins")) {
        profileMsg += static_cast<char16_t>(c);
    }
    profileMsg += u" L:";
    for (char c : player.at("losses")) {
        profileMsg += static_cast<char16_t>(c);
    }
    
    session->send(PacketBuilder::notification(profileMsg, 0));
}

void LobbyHandler::broadcastLobbyMessage(GameServer* server, const std::u16string& message, int32_t type) {
    auto lobbySessions = server->getLobbySessions();
    // system message 0xB4 is for server originated announcements
    auto pkt = PacketBuilder::systemMessage(message, type);
    for (const auto& s : lobbySessions) {
        s->send(pkt);
    }
}

std::vector<LobbyRoomInfo> LobbyHandler::getRoomList(GameServer* server) {
    std::vector<LobbyRoomInfo> result;
    
    const auto& rooms = server->rooms();
    for (const auto& [id, room] : rooms) {
        LobbyRoomInfo info;
        info.id = id;
        info.name = room->name();
        info.mode = static_cast<int>(room->settings().mode);
        info.playerCount = static_cast<int32_t>(room->playerCount());
        info.maxPlayers = room->settings().maxPlayers;
        info.mapId = room->settings().mapId;
        info.isPrivate = room->settings().isPrivate;
        info.isPlaying = room->isPlaying();
        result.push_back(info);
    }
    
    return result;
}

std::vector<LobbyPlayerInfo> LobbyHandler::getPlayerList(GameServer* server) {
    std::vector<LobbyPlayerInfo> result;
    
    auto sessions = server->getSessions();
    auto& db = Database::instance();
    
    for (const auto& s : sessions) {
        if (s->characterId == 0) continue;
        
        LobbyPlayerInfo info;
        info.id = s->characterId;
        info.name = s->characterName;
        info.inRoom = (s->roomId != 0);
        info.roomId = s->roomId;
        
        auto chars = db.queryPrepared(
            "SELECT level, wins, losses FROM characters WHERE id = ?",
            {std::to_string(s->characterId)});
        
        if (!chars.empty()) {
            info.level = std::stoi(chars[0]["level"]);
            info.wins = std::stoi(chars[0]["wins"]);
            info.losses = std::stoi(chars[0]["losses"]);
        } else {
            info.level = 1;
            info.wins = 0;
            info.losses = 0;
        }
        
        result.push_back(info);
    }
    
    return result;
}

} // namespace knc
