
#pragma once
#include <asio.hpp>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include "net/Session.h"
#include "net/Protocol.h"
#include "security/RateLimiter.h"
#include "handlers/AuthHandler.h"
#include "handlers/HandshakeHandler.h"

namespace knc {

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
    
    bool validateToken(const std::string& username, const std::string& token);
    int getAccountId(const std::string& username);

private:
    void startAccept();
    void handlePacket(Session::Ptr session, Packet& packet);
    void handleLauncherLogin(Session::Ptr session, Packet& packet);
    void handleClientInfo(Session::Ptr session, Packet& packet);
    void handleServerSelect(Session::Ptr session, Packet& packet);
    void handleGameServerRegister(Session::Ptr session, Packet& packet);
    void startCleanupTimer();
    void cleanupExpiredSessions();

    static std::string toHex(uint8_t val);

    asio::io_context m_ioContext;                          // must be declared before m cleanupTimer
    asio::ip::tcp::acceptor m_acceptor;
    asio::steady_timer m_cleanupTimer{m_ioContext};        // depends on m ioContext keep this order m acceptRetryTimer arms the acceptor again after a descriptor shortage
    asio::steady_timer m_acceptRetryTimer{m_ioContext};
    
    AuthHandler m_authHandler;
    HandshakeHandler m_handshakeHandler;
    
    // authenticated users keyed by username to session info
    std::unordered_map<std::string, AuthSession> m_authSessions;
    std::mutex m_authMutex;

    /// one flood throttle per socket keyed by session id and dropped on disconnect
    std::unordered_map<uint32_t, RateLimiter> m_rateLimiters;
};

} // namespace knc

