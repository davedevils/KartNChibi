/**
 * @file GameServer.h
 * @brief GameServer - handles game logic, rooms, races
 */

#pragma once
#include <asio.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "net/Session.h"
#include "net/Protocol.h"
#include "game/Room.h"
#include "packets/PacketBuilder.h"
#include "handlers/ShopHandler.h"
#include "handlers/RaceHandler.h"
#include "handlers/InventoryHandler.h"

namespace knc {

class GameServer {
public:
    explicit GameServer(int port);
    
    void run();
    void stop();
    
    // room management
    std::shared_ptr<Room> createRoom(const RoomSettings& settings);
    std::shared_ptr<Room> getRoom(uint32_t roomId);
    void removeRoom(uint32_t roomId);
    const std::unordered_map<uint32_t, std::shared_ptr<Room>>& rooms() const { return m_rooms; }
    
    // session management
    void addSession(Session::Ptr session);
    void removeSession(uint32_t sessionId);
    Session::Ptr getSession(uint32_t sessionId);
    size_t sessionCount() const { return m_sessions.size(); }
    
    // Get all sessions (for lobby broadcast)
    std::vector<Session::Ptr> getSessions() const {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        std::vector<Session::Ptr> result;
        result.reserve(m_sessions.size());
        for (const auto& [id, session] : m_sessions) {
            result.push_back(session);
        }
        return result;
    }
    
    // Get sessions in lobby (not in any room)
    std::vector<Session::Ptr> getLobbySessions() const {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        std::vector<Session::Ptr> result;
        for (const auto& [id, session] : m_sessions) {
            if (session->roomId == 0) {
                result.push_back(session);
            }
        }
        return result;
    }

private:
    void startAccept();
    void handlePacket(Session::Ptr session, Packet& packet);
    void onDisconnect(Session::Ptr session);
    void sendPlayerData(Session::Ptr session);
    
    // auth handlers
    void handleHeartbeat(Session::Ptr session, Packet& packet);
    void handleClientAuth(Session::Ptr session, Packet& packet);
    void handleFullState(Session::Ptr session, Packet& packet);
    void handleClientInfo(Session::Ptr session, Packet& packet);
    void handleSessionConfirm(Session::Ptr session, Packet& packet);
    
    // lobby handlers
    void handleChannelSelect(Session::Ptr session, Packet& packet);
    void handleLobbyRequest(Session::Ptr session, Packet& packet);
    void handleServerQuery(Session::Ptr session, Packet& packet);
    
    // room handlers
    void handleCreateRoom(Session::Ptr session, Packet& packet);
    void handleJoinRoom(Session::Ptr session, Packet& packet);
    void handleLeaveRoom(Session::Ptr session, Packet& packet);
    void handleRoomState(Session::Ptr session, Packet& packet);
    
    // chat handlers
    void handleChatMessage(Session::Ptr session, Packet& packet);
    void handleWhisper(Session::Ptr session, Packet& packet);
    void handleLobbyChat(Session::Ptr session, Packet& packet);
    
    // game handlers
    void handleStateChange(Session::Ptr session, Packet& packet);
    void handlePosition(Session::Ptr session, Packet& packet);
    void handleLapComplete(Session::Ptr session, Packet& packet);
    void handleItemPickup(Session::Ptr session, Packet& packet);
    void handleItemHit(Session::Ptr session, Packet& packet);
    void handleRaceFinish(Session::Ptr session, Packet& packet);
    void handleItemUse(Session::Ptr session, Packet& packet);
    void handleGameStart(Session::Ptr session, Packet& packet);
    void handlePlayerReady(Session::Ptr session, Packet& packet);
    
    // shop handlers
    void handleShopBrowse(Session::Ptr session, Packet& packet);
    void handleSellItem(Session::Ptr session, Packet& packet);
    
    // data handlers
    void handleRequestData(Session::Ptr session, Packet& packet);
    void handleUnknown32(Session::Ptr session, Packet& packet);
    
    // inventory handlers
    void handleEquipVehicle(Session::Ptr session, Packet& packet);
    void handleEquipAccessory(Session::Ptr session, Packet& packet);
    void handleUseItem(Session::Ptr session, Packet& packet);
    
    asio::io_context m_ioContext;
    asio::ip::tcp::acceptor m_acceptor;
    
    std::unordered_map<uint32_t, Session::Ptr> m_sessions;
    mutable std::mutex m_sessionsMutex;
    std::unordered_map<uint32_t, std::shared_ptr<Room>> m_rooms;
    mutable std::mutex m_roomsMutex;
    uint32_t m_nextRoomId = 1;
    
    // handlers
    ShopHandler m_shopHandler;
    RaceHandler m_raceHandler;
    InventoryHandler m_inventoryHandler;
};

} // namespace knc
