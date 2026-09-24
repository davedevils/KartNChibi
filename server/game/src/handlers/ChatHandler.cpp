/// handles chat packets 0x2D 0x2F and 0xB4

#include "handlers/ChatHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "logging/Logger.h"

namespace knc {

// CMD 0x2D room chat client sends a message server broadcasts sender id message and 7 int32 fields
void ChatHandler::handleChatMessage(Session::Ptr session, Packet& packet, GameServer* server) {
    std::u16string message = packet.readWString();

    if (message.empty()) return;

    std::string nameStr;
    for (char16_t c : session->characterName) {
        if (c > 0 && c < 128) nameStr += static_cast<char>(c);
    }

    if (session->roomId == 0) {
        LOG_WARN("CHAT", "CMD 0x2D from " + nameStr + " but not in a room");
        return;
    }

    auto room = server->getRoom(session->roomId);
    if (!room) {
        LOG_WARN("CHAT", "CMD 0x2D: room " + std::to_string(session->roomId) + " not found");
        return;
    }

    LOG_INFO("CHAT", "Room " + std::to_string(room->id()) + " chat from " + nameStr);

    // 0x2D is the lobby room list not chat real chat is 0xB4 sub 47CD60
    room->broadcast(PacketBuilder::lobbyChatBroadcast(
        static_cast<int32_t>(session->characterId), session->characterName, message, 0));
}

// CMD 0x2F whisper client sends target name and message server relays 0x2D or replies 0xB4 on failure
void ChatHandler::handleWhisper(Session::Ptr session, Packet& packet, GameServer* server) {
    std::u16string targetName = packet.readWString();
    std::u16string message    = packet.readWString();

    if (targetName.empty() || message.empty()) return;

    // find target session by character name snapshot taken under lock
    Session::Ptr targetSession = nullptr;
    auto allSessions = server->getSessions();
    for (const auto& sess : allSessions) {
        if (sess && sess->characterName == targetName) {
            targetSession = sess;
            break;
        }
    }

    if (targetSession) {
        sendRoomChat(targetSession, session->characterId, message);
        // Acknowledge the sender
        session->send(PacketBuilder::whisperEnable());

        std::string nameStr;
        for (char16_t c : session->characterName) {
            if (c > 0 && c < 128) nameStr += static_cast<char>(c);
        }
        LOG_INFO("CHAT", "Whisper from " + nameStr + " (char " +
                 std::to_string(session->characterId) + ") delivered");
    } else {
        session->send(PacketBuilder::systemMessage(u"Player not found.", 0));

        std::string targetStr;
        for (char16_t c : targetName) {
            if (c > 0 && c < 128) targetStr += static_cast<char>(c);
        }
        LOG_WARN("CHAT", "Whisper target not found: " + targetStr);
    }
}

// CMD 0xB4 lobby chat client sends a message and the server relays 0x2D to all lobby sessions
void ChatHandler::handleLobbyChat(Session::Ptr session, Packet& packet, GameServer* server) {
    std::u16string message = packet.readWString();

    if (message.empty()) return;

    std::string nameStr;
    for (char16_t c : session->characterName) {
        if (c > 0 && c < 128) nameStr += static_cast<char>(c);
    }
    LOG_INFO("CHAT", "Lobby chat from " + nameStr +
             " (char " + std::to_string(session->characterId) + ")");

    // broadcast to sessions with roomId 0 meaning in the lobby not in any room
    auto broadcastPkt = PacketBuilder::lobbyChatBroadcast(
        static_cast<int32_t>(session->characterId), session->characterName, message, 0);
    auto lobbySessions = server->getLobbySessions();  // already filtered by roomId 0
    for (const auto& sess : lobbySessions) {
        sess->send(broadcastPkt);
    }
}

void ChatHandler::sendRoomChat(Session::Ptr target, uint32_t senderId,
                               const std::u16string& message) {
    target->send(PacketBuilder::lobbyChatBroadcast(
        static_cast<int32_t>(senderId), u"", message, 0));
}

} // namespace knc
