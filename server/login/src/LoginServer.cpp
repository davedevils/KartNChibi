
#include "LoginServer.h"
#include "logging/Logger.h"
#include "net/ListenerHealth.h"
#include "config/Config.h"
#include "db/Database.h"
#include "security/PacketValidator.h"
#include "../../lib/crypto/PasswordHash.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace knc {

namespace {
// old leaked default rejected outright no server may boot with it
const char* const kLeakedInternalKey = "knc_internal_key_2025";
// the compose and env example value anyone can read so a public host must replace it
const char* const kSampleInternalKey = "change_this_to_a_random_string";

// reads the shared inter server key from the environment first then the config
std::string resolveInternalKey() {
    if (const char* env = std::getenv("KNC_INTERNAL_KEY")) {
        if (*env) return std::string(env);
    }
    return Config::instance().get<std::string>("internal.key", "");
}

// a login try runs PBKDF2 so 0x07 and 0xFE get a one second floor per socket
RateLimiter::Config loginRateConfig() {
    RateLimiter::Config cfg;
    cfg.globalMaxPerSec = RateLimit::GLOBAL_MAX_PACKETS_SEC;
    cfg.defaultMinIntervalMs = RateLimit::DEFAULT_MIN_INTERVAL;
    cfg.opcodeMinIntervalMs[CMD::C_HEARTBEAT] = 0;
    cfg.opcodeMinIntervalMs[CMD::C_CLIENT_AUTH] = RateLimit::LOGIN_MIN_INTERVAL;
    cfg.opcodeMinIntervalMs[CMD::C_LAUNCHER_LOGIN] = RateLimit::LOGIN_MIN_INTERVAL;
    return cfg;
}
}

LoginServer::LoginServer(int port)
    : m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
{
    // fail closed if the inter server key is missing or still the leaked default
    std::string internalKey = resolveInternalKey();
    if (internalKey.empty() || internalKey == kLeakedInternalKey) {
        LOG_ERROR("LOGIN", "KNC_INTERNAL_KEY is not set or still the old default, refusing to start");
        throw std::runtime_error("inter server key not configured");
    }
    if (internalKey == kSampleInternalKey) {
        LOG_WARN("LOGIN", "KNC_INTERNAL_KEY is the sample value, set a random one before a public start");
    }

    // clears servers table at startup game servers re register themselves
    auto& db = Database::instance();
    db.execute("DELETE FROM servers");
    LOG_INFO("LOGIN", "Cleared servers table - waiting for GameServer registrations");

    m_handshakeHandler.setTokenCheck([this](const std::string& user, const std::string& token) {
        return validateToken(user, token);
    });

    startAccept();
    startCleanupTimer();
}

void LoginServer::run() {
    LOG_INFO("LOGIN", "Server running...");
    m_ioContext.run();
}

void LoginServer::stop() {
    m_ioContext.stop();
    LOG_INFO("LOGIN", "Server stopped");
}

void LoginServer::startAccept() {
    m_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (ec) {
            if (acceptErrorIsFatal(ec)) {
                LOG_ERROR("LOGIN", "acceptor is gone: " + ec.message());
                return;
            }
            const int delay = acceptRetryDelayMs(ec);
            LOG_WARN("LOGIN", "accept failed: " + ec.message() +
                     " retry in " + std::to_string(delay) + " ms");
            if (delay > 0) {
                // out of descriptors arming at once would spin hot on the same error
                m_acceptRetryTimer.expires_after(std::chrono::milliseconds(delay));
                m_acceptRetryTimer.async_wait([this](const std::error_code& wec) {
                    if (!wec) startAccept();
                });
                return;
            }
            startAccept();
            return;
        }
        try {
            auto session = std::make_shared<Session>(std::move(socket));
            LOG_INFO("LOGIN", "New connection from " + session->remoteAddress());
            
            session->setPacketHandler([this](Session::Ptr s, Packet& pkt) {
                handlePacket(s, pkt);
            });
            
            session->setDisconnectHandler([this](Session::Ptr s) {
                LOG_INFO("LOGIN", "Client disconnected: " + s->remoteAddress());
                m_rateLimiters.erase(s->id());
            });

            m_rateLimiters.emplace(session->id(), RateLimiter(loginRateConfig()));
            session->setIdleTimeout(std::chrono::seconds(SESSION_IDLE_LIMIT_SEC));
            session->start();
            
            // wait for the client to send 0xFA before sending 0x0A since it always leads with 0xA6
        } catch (const std::exception& e) {
            LOG_ERROR("LOGIN", std::string("accept path threw: ") + e.what());
        } catch (...) {
            LOG_ERROR("LOGIN", "accept path threw an unknown error");
        }

        startAccept();
    });
}

void LoginServer::handlePacket(Session::Ptr session, Packet& packet) {
    std::string hexDump = "OP=0x" + toHex(packet.flag()) + toHex(packet.cmd()) +
                          " Size=" + std::to_string(packet.payloadSize());

    // 0x07 0xFE 0xD0 0xA7 carry a password or a token and 0xF0 the inter server key
    if (!packet.payload().empty() && !PacketValidator::payloadIsSecret(packet.opcode())) {
        hexDump += " Data=[";
        for (size_t i = 0; i < std::min(size_t(32), packet.payload().size()); ++i) {
            hexDump += toHex(packet.payload()[i]);
            if (i < 31 && i < packet.payload().size() - 1) hexDump += " ";
        }
        if (packet.payload().size() > 32) hexDump += "...";
        hexDump += "]";
    }

    LOG_INFO("LOGIN", "RECV from " + session->remoteAddress() + ": " + hexDump);

    const ValidationResult vr = PacketValidator::validate(packet);
    if (vr != ValidationResult::OK) {
        LOG_WARN("LOGIN", "drop invalid packet from " + session->remoteAddress() + " reason " +
                 PacketValidator::resultToString(vr));
        return;
    }
    auto rit = m_rateLimiters.find(session->id());
    if (rit != m_rateLimiters.end() && !rit->second.check(packet.opcode())) {
        LOG_WARN("LOGIN", "rate limit drop from " + session->remoteAddress() +
                 " total dropped " + std::to_string(rit->second.getDroppedCount()));
        return;
    }

    // the full u16 opcode or a 0x01xx frame lands on the case of its low byte
    switch (packet.opcode()) {
        case CMD::C_HEARTBEAT:
            m_authHandler.handleHeartbeat(session, packet);
            break;
            
        case CMD::C_FULL_STATE:
            m_handshakeHandler.handleFullState(session, packet);
            break;
            
        case CMD::C_CLIENT_AUTH:
            m_handshakeHandler.handleClientAuth(session, packet);
            break;

        // 0xA7 channel return sub 483FF0 sends instead of 0x07 after first auth new socket no 0xFA needs no handshake state
        case CMD::S_SESSION_CONFIRM:
            m_handshakeHandler.handleReauth(session, packet);
            break;
            
        case CMD::C_CLIENT_INFO:  // 0xD0 client sends IP and display info
            handleClientInfo(session, packet);
            break;
        
        case 0x8E:  // Client ACK - just ignore
            LOG_DEBUG("LOGIN", "Client ACK received");
            break;
            
        // option 11 float the low byte switch used to answer it with a room 0x30
        case CMD::C_LOBBY_TELEMETRY:
            break;
        
        case CMD::C_SERVER_QUERY:  // 0x19 client selected play or server
            handleServerSelect(session, packet);
            break;
        
        case 0x18:  // 0x18 client selected a channel from state 4
            {
                if (session->accountId == 0) {
                    LOG_WARN("LOGIN", "channel select before login from " + session->remoteAddress());
                    break;
                }
                // screenCmd of 0x0E means the request came from channel select
                int32_t screenCmd = packet.readInt32();
                int32_t channelId = packet.readInt32();
                LOG_INFO("LOGIN", "Channel selection: screen=0x" + toHex(screenCmd) + 
                         " channelId=" + std::to_string(channelId));
                
                if (screenCmd == 0x0E) {
                    auto& db = Database::instance();

                    auto chars = db.queryPrepared(
                        "SELECT id FROM characters WHERE account_id = ? LIMIT 1",
                        {std::to_string(session->accountId)}
                    );
                    
                    // redirects to GameServer regardless of character existence since it detects driver id negative one
                    if (chars.empty()) {
                        LOG_INFO("LOGIN", "No character for account " + std::to_string(session->accountId) + 
                                 " - redirecting to GameServer for character creation");
                    }

                    auto servers = db.query(
                        "SELECT host, port FROM servers WHERE type = 'game' AND is_online = 1 LIMIT 1"
                    );
                    
                    if (!servers.empty()) {
                        std::string gameHost = servers[0]["host"];
                        int gamePort = std::stoi(servers[0]["port"]);
                        LOG_INFO("LOGIN", "Redirecting to GameServer " + gameHost + ":" + std::to_string(gamePort));
                        m_handshakeHandler.sendServerRedirect(session, gameHost, gamePort, session->sessionToken);
                    } else {
                        LOG_ERROR("LOGIN", "No GameServer available!");
                    }
                }
            }
            break;
            
        case CMD::C_LAUNCHER_LOGIN:  // 0xFE - Launcher login
            handleLauncherLogin(session, packet);
            break;
        
        case 0xF0:  // GameServer registration
            handleGameServerRegister(session, packet);
            break;
            
        default:
            LOG_WARN("LOGIN", "Unhandled opcode 0x" + toHex(packet.flag()) + toHex(packet.cmd()) +
                     " (" + std::to_string(packet.payload().size()) + " bytes payload)");
            break;
    }
}

void LoginServer::handleLauncherLogin(Session::Ptr session, Packet& packet) {
    // launcher login payload is size then 0xFE then username then password
    std::string username = packet.readString(32);
    std::string password = packet.readString(32);
    
    LOG_INFO("LOGIN", "Launcher login: " + username + " from " + session->remoteAddress());
    
    auto& config = Config::instance();
    auto& db = Database::instance();
    bool autoCreate = config.get("auth.auto_create_accounts", false);
    
    bool success = false;
    std::string message = "Invalid credentials";
    int accountId = 0;
    
    if (username.empty() || password.empty()) {
        message = "Username and password required";
    } else {
        auto results = db.queryPrepared(
            "SELECT id, password_hash, is_banned FROM accounts WHERE username = ?",
            {username});

        if (results.empty() && autoCreate) {
            std::string hash = PasswordHash::hash(password);
            if (db.executePrepared(
                    "INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
                    {username, hash})) {
                results = db.queryPrepared(
                    "SELECT id, password_hash, is_banned FROM accounts WHERE username = ?",
                    {username});
                LOG_INFO("LOGIN", "Auto-created account: " + username);
            }
        }
        
        if (!results.empty()) {
            auto& row = results[0];
            
            if (row["is_banned"] == "1") {
                message = "Account is banned";
            } else {
                // verifies password supporting both pbkdf2 and legacy sha256 formats
                bool passOk = PasswordHash::verify(password, row["password_hash"]);

                if (passOk) {
                    success = true;
                    accountId = std::stoi(row["id"]);
                    message = "Welcome!";

                    // auto upgrades a legacy sha256 hash to PBKDF2 on first successful login
                    if (PasswordHash::isLegacy(row["password_hash"])) {
                        db.executePrepared(
                            "UPDATE accounts SET password_hash = ? WHERE id = ?",
                            {PasswordHash::hash(password), row["id"]});
                        LOG_INFO("LOGIN", "Upgraded legacy password hash for: " + username);
                    }

                    db.executePrepared("UPDATE accounts SET last_login = NOW() WHERE id = ?", {row["id"]});

                    session->accountId = accountId;
                    session->sessionToken = m_authHandler.generateToken();
                }
            }
        }
    }
    
    if (success) {
        std::lock_guard<std::mutex> lock(m_authMutex);
        m_authSessions[username] = {
            accountId,
            session->sessionToken,
            std::chrono::steady_clock::now()
        };
    }
    
    // response carries success then token then message with token sent only on success
    Packet response(CMD::S_LAUNCHER_RESPONSE);
    response.writeUInt8(success ? 0x01 : 0x00);
    response.writeString(success ? session->sessionToken : "");
    response.writeString(message);
    session->send(response);
    
    if (success) {
        LOG_INFO("LOGIN", "Launcher auth success: " + username + " (ID: " + std::to_string(accountId) + 
                 ") token=" + session->sessionToken.substr(0, 8) + "...");
    } else {
        LOG_WARN("LOGIN", "Launcher auth failed: " + username + " - " + message);
    }
}

void LoginServer::handleClientInfo(Session::Ptr session, Packet& packet) {
    // 0xD0 payload is a token wstring then an ip string
    LOG_INFO("LOGIN", "Client info received from " + session->remoteAddress());
    
    // parses the token from the packet as a UTF 16LE wstring
    std::string token;
    if (packet.remaining() >= 2) {
        std::u16string tokenW = packet.readWString(64);
        for (char16_t c : tokenW) {
            if (c == 0) break;
            if (c < 128) token += static_cast<char>(c);
        }
    }
    
    LOG_DEBUG("LOGIN", "Client token: '" + token + "'");
    
    // token format is session then username so the username is extracted from it
    std::string username;
    const std::string prefix = "session_";
    if (token.find(prefix) == 0) {
        username = token.substr(prefix.length());
    }
    
    if (!username.empty() && validateToken(username, token)) {
        session->launcherAuthenticated = true;
        session->authenticatedUser = username;
        session->accountId = getAccountId(username);
        LOG_INFO("LOGIN", "Token validated for user: " + username + " (ID: " + std::to_string(session->accountId) + ")");
    } else {
        session->launcherAuthenticated = false;
        LOG_WARN("LOGIN", "Invalid or missing token from " + session->remoteAddress());
    }
    
    Packet ack(CMD::S_ACK);
    session->send(ack);
    LOG_DEBUG("LOGIN", "Sent ACK for client info");
}

void LoginServer::handleServerSelect(Session::Ptr session, Packet& packet) {
    // 0x19 channel selected redirects to GameServer which handles character creation if needed
    if (session->accountId == 0) {
        LOG_WARN("LOGIN", "server select before login from " + session->remoteAddress());
        return;
    }

    LOG_INFO("LOGIN", "Channel selected by " + session->remoteAddress() + 
             " (account=" + std::to_string(session->accountId) + ")");
    
    // packet format is ip 11 bytes then port 4 bytes then 4 unknown bytes
    std::string clientIp = packet.readString(11);
    int32_t clientPort = packet.readInt32();
    LOG_DEBUG("LOGIN", "Client info: " + clientIp + ":" + std::to_string(clientPort));
    
    // looks up GameServer info from the DB using the first available server for now
    auto& db = Database::instance();
    auto servers = db.query(
        "SELECT host, port FROM servers WHERE type = 'game' AND is_online = 1 LIMIT 1"
    );
    
    if (servers.empty()) {
        LOG_ERROR("LOGIN", "No GameServer available!");
        Packet resp(CMD::S_LOGIN_RESPONSE);
        resp.writeString("No server available");
        resp.writeInt32(1);  // Error code
        session->send(resp);
        return;
    }
    
    std::string gameHost = servers[0]["host"];
    int gamePort = std::stoi(servers[0]["port"]);
    
    LOG_INFO("LOGIN", "Redirecting to GameServer " + gameHost + ":" + std::to_string(gamePort));
    m_handshakeHandler.sendServerRedirect(session, gameHost, gamePort, session->sessionToken);
}

std::string LoginServer::toHex(uint8_t val) {
    const char* hex = "0123456789ABCDEF";
    return std::string(1, hex[val >> 4]) + hex[val & 0xF];
}

bool LoginServer::validateToken(const std::string& username, const std::string& token) {
    std::lock_guard<std::mutex> lock(m_authMutex);
    
    auto it = m_authSessions.find(username);
    if (it == m_authSessions.end()) {
        LOG_WARN("LOGIN", "Token validation failed: user '" + username + "' not authenticated");
        return false;
    }
    
    if (it->second.token != token) {
        LOG_WARN("LOGIN", "Token validation failed: invalid token for '" + username + "'");
        return false;
    }
    
    // checks the session has not expired past 30 minutes
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.loginTime);
    if (elapsed.count() > 30) {
        LOG_WARN("LOGIN", "Token validation failed: session expired for '" + username + "'");
        m_authSessions.erase(it);
        return false;
    }
    
    return true;
}

int LoginServer::getAccountId(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_authMutex);
    
    auto it = m_authSessions.find(username);
    if (it != m_authSessions.end()) {
        return it->second.accountId;
    }
    return 0;
}

void LoginServer::handleGameServerRegister(Session::Ptr session, Packet& packet) {
    // registration packet format is key id name host port maxPlayers then type

    std::string expectedKey = resolveInternalKey();

    std::string key = packet.readString(64);
    if (key != expectedKey) {
        LOG_WARN("LOGIN", "GameServer registration rejected: invalid key from " + session->remoteAddress());
        session->stop();
        return;
    }
    
    int32_t serverId = packet.readInt32();
    std::string name = packet.readString(64);
    std::string host = packet.readString(64);
    int32_t port = packet.readInt32();
    int32_t maxPlayers = packet.readInt32();
    int32_t serverType = packet.readInt32();
    
    LOG_INFO("LOGIN", "GameServer registering: " + name + " (" + host + ":" + std::to_string(port) + ")");
    
    auto& db = Database::instance();

    bool dbOk = db.executePrepared(
        "INSERT INTO servers (id, name, host, port, type, max_players, is_online) VALUES (?, ?, ?, ?, 'game', ?, 1) "
        "ON DUPLICATE KEY UPDATE name=?, host=?, port=?, max_players=?, is_online=1",
        {std::to_string(serverId), name, host, std::to_string(port), std::to_string(maxPlayers),
         name, host, std::to_string(port), std::to_string(maxPlayers)});

    if (dbOk) {
        LOG_INFO("LOGIN", "GameServer registered: " + name + " (ID: " + std::to_string(serverId) + ")");

        Packet resp(0xF0);
        resp.writeUInt8(1);  // Success
        session->send(resp);

        // marks this session as a GameServer connection reusing the Redirected state
        session->handshakeState = Session::HandshakeState::Redirected;
    } else {
        LOG_ERROR("LOGIN", "Failed to register GameServer: " + name);
        
        Packet resp(0xF0);
        resp.writeUInt8(0);  // Failure
        session->send(resp);
    }
}

void LoginServer::startCleanupTimer() {
    m_cleanupTimer.expires_after(std::chrono::seconds(60));
    m_cleanupTimer.async_wait([this](const std::error_code& ec) {
        if (!ec) {
            cleanupExpiredSessions();
            startCleanupTimer();
        }
    });
}

void LoginServer::cleanupExpiredSessions() {
    // cleans the in memory map of entries older than 30 minutes
    {
        std::lock_guard<std::mutex> lock(m_authMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_authSessions.begin(); it != m_authSessions.end(); ) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                now - it->second.loginTime).count();
            if (elapsed > 30) {
                it = m_authSessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    // cleans DB active sessions older than 5 minutes
    auto& db = Database::instance();
    db.execute("DELETE FROM active_sessions WHERE created_at < DATE_SUB(NOW(), INTERVAL 5 MINUTE)");

    LOG_DEBUG("LOGIN", "Cleanup: expired sessions purged");
}

} // namespace knc

