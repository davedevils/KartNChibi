/// handles chat packets 0x2D 0x2F and 0xB4

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class ChatHandler {
public:
    /// CMD 0x2D room chat parses a wstring and broadcasts to room members
    void handleChatMessage(Session::Ptr session, Packet& packet, GameServer* server);

    /// CMD 0x2F whisper carries a target name wstring and a message wstring routed to the target
    void handleWhisper(Session::Ptr session, Packet& packet, GameServer* server);

    /// CMD 0xB4 lobby chat broadcasts to all sessions with room id zero
    void handleLobbyChat(Session::Ptr session, Packet& packet, GameServer* server);

private:
    /// build and send a CMD 0x2D chat packet to a single session
    void sendRoomChat(Session::Ptr target, uint32_t senderId,
                      const std::u16string& message);
};

} // namespace knc
