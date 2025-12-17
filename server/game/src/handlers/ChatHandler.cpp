/**
 * @file ChatHandler.cpp
 * @brief Handles chat packets
 */

#include "handlers/ChatHandler.h"
#include "GameServer.h"
#include "logging/Logger.h"

namespace knc {

void ChatHandler::handleChatMessage(Session::Ptr session, Packet& packet, GameServer* server) {
    // Read chat message structure (~116 bytes)
    // int32 senderId, wchar_t[42] message, int32[7] unknowns, int32 channel
    
    uint32_t senderId = packet.readUInt32();
    std::u16string message = packet.readWString(42);
    
    // Skip unknowns (7 * 4 = 28 bytes)
    for (int i = 0; i < 7; ++i) {
        packet.readInt32();
    }
    
    uint8_t channel = static_cast<uint8_t>(packet.readInt32());
    
    LOG_INFO("CHAT", "Message from " + std::to_string(senderId) + " channel " + std::to_string(channel));
    
    // Broadcast to all connected players (public chat)
    if (channel == 0x08) {  // Public chat
        for (auto& [id, sess] : server->rooms()) {
            // For now broadcast to all sessions
        }
        
        // Send to all sessions
        Packet chatPkt(CMD::S_CHAT_MESSAGE);
        chatPkt.writeUInt32(session->id());  // Use session ID as sender
        
        // Write message as UTF-16LE (84 bytes max)
        for (size_t i = 0; i < 42; ++i) {
            if (i < message.size()) {
                chatPkt.writeUInt16(static_cast<uint16_t>(message[i]));
            } else {
                chatPkt.writeUInt16(0);
            }
        }
        
        // Unknowns
        for (int i = 0; i < 7; ++i) {
            chatPkt.writeInt32(0);
        }
        
        chatPkt.writeInt32(channel);
        
        // Broadcast to all lobby sessions
        auto data = chatPkt.serialize();
        for (auto& sess : server->getLobbySessions()) {
            if (sess) sess->send(data);
        }
    }
}

void ChatHandler::handleWhisper(Session::Ptr session, Packet& packet, GameServer* server) {
    // Private message - Format: targetName (wstring) + message (wstring)
    std::u16string targetName = packet.readWString();
    std::u16string message = packet.readWString();
    
    // Convert target name to string for lookup
    std::string targetNameStr;
    for (char16_t c : targetName) {
        if (c > 0 && c < 128) targetNameStr += static_cast<char>(c);
    }
    
    // Find target session by character name
    Session::Ptr targetSession = nullptr;
    for (auto& sess : server->getLobbySessions()) {
        if (sess && sess->characterName == targetName) {
            targetSession = sess;
            break;
        }
    }
    
    if (targetSession) {
        // Send whisper to target
        sendChatMessage(targetSession, session->characterId, message, 0x01);  // 0x01 = whisper channel
        LOG_INFO("CHAT", "Whisper from " + std::to_string(session->characterId) + 
                 " to " + targetNameStr + ": sent");
    } else {
        // Target not found - send error message back
        LOG_WARN("CHAT", "Whisper target not found: " + targetNameStr);
        // Could send error notification here
    }
}

void ChatHandler::sendChatMessage(Session::Ptr target, uint32_t senderId,
                                   const std::u16string& message, uint8_t channel) {
    Packet pkt(CMD::S_CHAT_MESSAGE);
    pkt.writeUInt32(senderId);
    
    for (size_t i = 0; i < 42; ++i) {
        if (i < message.size()) {
            pkt.writeUInt16(static_cast<uint16_t>(message[i]));
        } else {
            pkt.writeUInt16(0);
        }
    }
    
    for (int i = 0; i < 7; ++i) {
        pkt.writeInt32(0);
    }
    
    pkt.writeInt32(channel);
    
    target->send(pkt);
}

} // namespace knc
