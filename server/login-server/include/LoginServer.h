/**
 * @file LoginServer.h
 * @brief LoginServer - handles authentication and redirects to GameServer
 */

#pragma once
#include <asio.hpp>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include "net/Session.h"
#include "net/Protocol.h"
#include "handlers/AuthHandler.h"
#include "handlers/HandshakeHandler.h"

namespace knc {

// Authenticated session info
struct AuthSession {
    int accountId;
    std::string token;
    std::chrono::steady_clock::time_point loginTime;
};

class LoginServer {
public:
    explicit LoginServer(int port);
    
    void run();
    void stop();
    
    // Token validation
    bool validateToken(const std::string& username, const std::string& token);
    int getAccountId(const std::string& username);

private:
    void startAccept();
    void handlePacket(Session::Ptr session, Packet& packet);
    void handleLauncherLogin(Session::Ptr session, Packet& packet);
    void handleClientInfo(Session::Ptr session, Packet& packet);
    void handleServerSelect(Session::Ptr session, Packet& packet);
    void handleGameServerRegister(Session::Ptr session, Packet& packet);
    
    static std::string toHex(uint8_t val);
    
    asio::io_context m_ioContext;
    asio::ip::tcp::acceptor m_acceptor;
    
    AuthHandler m_authHandler;
    HandshakeHandler m_handshakeHandler;
    
    // Authenticated users: username → session info
    std::unordered_map<std::string, AuthSession> m_authSessions;
    std::mutex m_authMutex;
};

} // namespace knc

