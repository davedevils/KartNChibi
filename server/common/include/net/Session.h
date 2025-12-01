/**
 * @file Session.h
 * @brief Async TCP session with ASIO
 */

#pragma once
#include <asio.hpp>
#include <memory>
#include <vector>
#include <queue>
#include <functional>
#include "Packet.h"

namespace knc {

class Session : public std::enable_shared_from_this<Session> {
public:
    using Ptr = std::shared_ptr<Session>;
    using PacketHandler = std::function<void(Session::Ptr, Packet&)>;
    using DisconnectHandler = std::function<void(Session::Ptr)>;
    
    Session(asio::ip::tcp::socket socket);
    ~Session();
    
    void start();
    void stop();
    
    void send(const Packet& packet);
    void send(const std::vector<uint8_t>& data);
    
    // Handlers
    void setPacketHandler(PacketHandler handler) { m_packetHandler = handler; }
    void setDisconnectHandler(DisconnectHandler handler) { m_disconnectHandler = handler; }
    
    // Info
    uint32_t id() const { return m_id; }
    std::string remoteAddress() const;
    uint16_t remotePort() const;
    bool isConnected() const { return m_connected; }
    
    // Session data
    uint32_t accountId = 0;
    uint32_t characterId = 0;
    std::string sessionToken;
    std::string authenticatedUser;  // Username from valid launcher login
    bool launcherAuthenticated = false;  // True if validated via launcher
    
    // Handshake state
    enum class HandshakeState { 
        Initial, 
        AwaitingAuth, 
        AwaitingCharacterCreation,  // No character - waiting for creation packet
        ChannelListSent, 
        Redirected 
    };
    HandshakeState handshakeState = HandshakeState::Initial;

private:
    void doRead();
    void doWrite();
    void processBuffer();
    
    asio::ip::tcp::socket m_socket;
    uint32_t m_id;
    bool m_connected = false;
    
    std::vector<uint8_t> m_readBuffer;
    std::vector<uint8_t> m_recvBuffer;
    std::queue<std::vector<uint8_t>> m_writeQueue;
    bool m_writing = false;
    
    PacketHandler m_packetHandler;
    DisconnectHandler m_disconnectHandler;
    
    static uint32_t s_nextId;
};

} // namespace knc

