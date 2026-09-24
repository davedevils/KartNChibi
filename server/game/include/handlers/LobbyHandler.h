/// handles lobby packets quick match player profile friends and block list

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>
#include <string>

namespace knc {

class GameServer;

struct LobbyPlayerInfo {
    uint32_t id;
    std::u16string name;
    int32_t level;
    int32_t wins;
    int32_t losses;
    bool inRoom;
    int32_t roomId;
};

struct LobbyRoomInfo {
    uint32_t id;
    std::string name;
    int32_t mode;
    int32_t playerCount;
    int32_t maxPlayers;
    int32_t mapId;
    bool isPrivate;
    bool isPlaying;
};

class LobbyHandler {
public:
    static void handleRoomListRequest(Session::Ptr session, GameServer* server);
    // C2S 0x64 from the lobby one int32 mode seats the player through GameServer quickMatch
    static void handleQuickMatch(Session::Ptr session, Packet& packet, GameServer* server);

    static void handlePlayerListRequest(Session::Ptr session, GameServer* server);
    static void handlePlayerProfile(Session::Ptr session, Packet& packet, GameServer* server);

    static void handleLobbyChat(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleWhisper(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleAddFriend(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleRemoveFriend(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleBlockPlayer(Session::Ptr session, Packet& packet, GameServer* server);

    static void sendRoomList(Session::Ptr session, GameServer* server);
    static void sendPlayerList(Session::Ptr session, GameServer* server);
    static void sendPlayerProfile(Session::Ptr session, int32_t playerId);
    static void broadcastLobbyMessage(GameServer* server, const std::u16string& message, int32_t type);
    
    static std::vector<LobbyRoomInfo> getRoomList(GameServer* server);
    static std::vector<LobbyPlayerInfo> getPlayerList(GameServer* server);
};

} // namespace knc
