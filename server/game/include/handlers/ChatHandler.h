/**
 * @file ChatHandler.h
 * @brief Handles chat packets (0x2D, 0x2F)
 */

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class ChatHandler {
public:
    void handleChatMessage(Session::Ptr session, Packet& packet, GameServer* server);
    void handleWhisper(Session::Ptr session, Packet& packet, GameServer* server);
    
    void sendChatMessage(Session::Ptr target, uint32_t senderId, 
                         const std::u16string& message, uint8_t channel);
};

} // namespace knc
