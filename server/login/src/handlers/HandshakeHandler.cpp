/// @brief handshake 0xFA 0x07 0x0A

#include "handlers/HandshakeHandler.h"
#include "logging/Logger.h"
#include "config/Config.h"
#include "db/Database.h"
#include "../../../lib/crypto/PasswordHash.h"
#include "net/ProfileBlob.h"
#include <cstdlib>

// lives in LoginServer cannot call MissionHandler game only

namespace knc {

void HandshakeHandler::handleFullState(Session::Ptr session, Packet&) {
    LOG_INFO("HANDSHAKE", "Received FULL_STATE (0xFA) from " + session->remoteAddress() +
             " state=" + std::to_string(static_cast<int>(session->handshakeState)));

    if (session->handshakeState == Session::HandshakeState::ChannelListSent) {
        Packet ack(CMD::S_ACK);
        session->send(ack);
        LOG_DEBUG("HANDSHAKE", "Sent 0x0B ACK for Channel Select state");
        return;
    }

    if (session->handshakeState == Session::HandshakeState::Redirected) {
        LOG_DEBUG("HANDSHAKE", "Ignoring 0xFA - already redirected");
        return;
    }

    Packet ack(CMD::S_ACK);
    session->send(ack);

    session->handshakeState = Session::HandshakeState::AwaitingAuth;
    LOG_DEBUG("HANDSHAKE", "Sent 0x0B ACK, waiting for 0x07 (Client Auth)");
}

void HandshakeHandler::handleClientAuth(Session::Ptr session, Packet& packet) {
    LOG_INFO("HANDSHAKE", "Received CLIENT_AUTH (0x07) from " + session->remoteAddress() +
             " (" + std::to_string(packet.payloadSize()) + " bytes)");

    // launcher mode minimal packet direct mode has user pass
    std::string username = "OGPlayer";
    std::string password;
    int32_t action = 0;
    std::string version;

    if (packet.remaining() >= 4) {
        version = packet.readString(4);
        LOG_DEBUG("HANDSHAKE", "Client version: " + version);
    }

    if (packet.remaining() >= 4) {
        action = packet.readInt32();
        LOG_DEBUG("HANDSHAKE", "Action: " + std::to_string(action));
    }

    if (packet.remaining() >= 4) {
        std::u16string usernameW = packet.readWString(32);
        std::u16string passwordW = packet.readWString(32);

        username.clear();
        for (char16_t c : usernameW) {
            if (c == 0) break;
            if (c < 128) username += static_cast<char>(c);
            else username += '?';
        }
        for (char16_t c : passwordW) {
            if (c == 0) break;
            if (c < 128) password += static_cast<char>(c);
            else password += '?';
        }
    }
    
    LOG_INFO("HANDSHAKE", "Client auth: user='" + username + "' action=" + std::to_string(action));
    
    bool loginSuccess = false;
    
    if (!username.empty() && !password.empty() && action == 4) {
        // Direct login mode - validate credentials
        loginSuccess = validateLogin(session, username, password);
    } else if (session->launcherAuthenticated) {
        // OGPlanet mode - client authenticated via launcher
        LOG_INFO("HANDSHAKE", "OGPlanet mode - user '" + session->authenticatedUser + "' validated");
        // accountId already set in handleClientInfo
        loginSuccess = true;
    } else {
        // rejects the connection when not authenticated via the launcher
        LOG_WARN("HANDSHAKE", "Connection rejected: not authenticated via launcher");

        Packet errorResp(CMD::S_LOGIN_RESPONSE);
        errorResp.writeString("MSG_REINPUT_IDPASS");
        errorResp.writeInt32(1);  // Error code
        session->send(errorResp);

        session->closeAfterSend();
        return;
    }
    
    // a failed validateLogin left accountId at 0 and served character creation this rejects it properly
    if (!loginSuccess || session->accountId == 0) {
        LOG_WARN("HANDSHAKE", "Login refused for user '" + username + "'");
        Packet errorResp(CMD::S_LOGIN_RESPONSE);
        errorResp.writeString("MSG_REINPUT_IDPASS");
        errorResp.writeInt32(1);
        session->send(errorResp);
        session->closeAfterSend();
        return;
    }

    // one account one game at a time a stale heartbeat row lets the player back in
    {
        auto live = Database::instance().queryPrepared(
            "SELECT 1 FROM online_players WHERE account_id = ? AND last_seen > NOW() - INTERVAL 90 SECOND",
            {std::to_string(session->accountId)});
        if (!live.empty()) {
            LOG_WARN("HANDSHAKE", "Login refused, account " + std::to_string(session->accountId) +
                     " is already in game");
            sendDisplayMessage(session, u"That account is already logged in.", 2);
            session->closeAfterSend();
            return;
        }
    }

    // 0x07 goes out with or without a character the creation popup is the game server after the 0xA7
    LOG_INFO("HANDSHAKE", "Sending session info (0x07)");
    sendSessionInfo(session);

    // the empty 0x02 type 1 sets dword F727F4 to 1 for connection confirmed on both paths
    LOG_INFO("HANDSHAKE", "Sending connection confirmation (0x02) with code=1");
    sendDisplayMessage(session, u"", 1);
    
    LOG_INFO("HANDSHAKE", "Sending channel list (0x0E)");
    sendServerList(session);
    session->handshakeState = Session::HandshakeState::ChannelListSent;
    
    LOG_INFO("HANDSHAKE", "Login complete - waiting for channel selection from " + session->remoteAddress());
}

void HandshakeHandler::sendServerList(Session::Ptr session) {
    // CMD 0x0E channel select from sub 4793F0 count then each channel id name players type footer
    
    Packet pkt(CMD::S_CHANNEL_LIST, 0x00);

    // reads servers from the db the only source of truth an empty table sends zero channels
    auto& db = Database::instance();
    auto servers = db.queryPrepared(
        "SELECT id, name, max_players FROM servers WHERE type = 'game' AND is_online = 1 ORDER BY id",
        {}
    );

    if (servers.empty()) {
        LOG_WARN("HANDSHAKE", "No game servers in DB sending empty channel list");
        pkt.writeInt32(0);
    } else {
        pkt.writeInt32(static_cast<int32_t>(servers.size()));
        
        for (const auto& server : servers) {
            int serverId = std::stoi(server.at("id"));
            std::string serverName = server.at("name");
            int maxPlayers = std::stoi(server.at("max_players"));
            
            // channel ids are 0 and 2 rookie 4 and 5 advanced 6 and 8 master
            int channelId = (serverId - 1) * 2;
            
            pkt.writeInt32(channelId);
            
            // Name as wstring UTF-16LE
            for (char c : serverName) {
                pkt.writeUInt8(static_cast<uint8_t>(c));
                pkt.writeUInt8(0);
            }
            pkt.writeUInt8(0);
            pkt.writeUInt8(0);
            
            pkt.writeInt32(0);          // currentPlayers
            pkt.writeInt32(maxPlayers);
            pkt.writeInt32(0);
        }
    }
    
    // footer wstring sits after all channels
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    
    session->send(pkt);
    LOG_INFO("HANDSHAKE", "Sent channel list with " + std::to_string(servers.empty() ? 1 : servers.size()) + " channels");
}

bool HandshakeHandler::validateLogin(Session::Ptr session, const std::string& username, 
                                      const std::string& password) {
    auto& config = Config::instance();
    auto& db = Database::instance();
    
    bool autoCreate = config.get("auth.auto_create_accounts", false);
    bool allowTest = config.get("auth.allow_test_login", false);
    
    LOG_DEBUG("HANDSHAKE", "validateLogin: user='" + username + "' pass=***");

    // any 32 char password used to pass here unchecked now the launcher store must hold it else the hash decides
    if (launcherTokenAccepted(username, password, m_tokenCheck)) {
        auto results = db.queryPrepared("SELECT id, is_banned FROM accounts WHERE username = ?", {username});
        if (!results.empty() && results[0]["is_banned"] != "1") {
            session->accountId = std::stoi(results[0]["id"]);
            session->sessionToken = password;
            LOG_INFO("HANDSHAKE", "launcher token login: " + username + " (ID: " +
                     std::to_string(session->accountId) + ")");
            return true;
        }
    }

    if (allowTest && username == "test" && password == "test") {
        session->accountId = 1;
        session->sessionToken = m_authHandler.generateToken();
        return true;
    }
    
    if (username.empty() || password.empty()) {
        return false;
    }
    
    auto results = db.queryPrepared(
        "SELECT id, password_hash, is_banned FROM accounts WHERE username = ?",
        {username});

    if (results.empty() && autoCreate) {
        // hashes with PBKDF2-SHA256 never store plaintext with a sha256 prefix
        std::string hash;
        try {
            hash = PasswordHash::hash(password);
        } catch (const std::exception& e) {
            LOG_ERROR("HANDSHAKE", std::string("PasswordHash::hash failed: ") + e.what());
            return false;
        }
        if (db.executePrepared(
                "INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
                {username, hash})) {
            results = db.queryPrepared(
                "SELECT id, password_hash, is_banned FROM accounts WHERE username = ?",
                {username});
            LOG_INFO("HANDSHAKE", "Auto-created account: " + username);
        }
    }

    if (!results.empty()) {
        auto& row = results[0];

        if (row["is_banned"] == "1") {
            return false;
        }

        if (PasswordHash::verify(password, row["password_hash"])) {
            session->accountId = std::stoi(row["id"]);
            session->sessionToken = m_authHandler.generateToken();
            db.executePrepared("UPDATE accounts SET last_login = NOW() WHERE id = ?", {row["id"]});
            // transparently upgrades legacy sha256 prefixed hashes to PBKDF2
            if (PasswordHash::isLegacy(row["password_hash"])) {
                try {
                    std::string upgraded = PasswordHash::hash(password);
                    db.executePrepared(
                        "UPDATE accounts SET password_hash = ? WHERE id = ?",
                        {upgraded, row["id"]});
                } catch (const std::exception& e) {
                    LOG_WARN("HANDSHAKE", std::string("Legacy hash upgrade failed: ") + e.what());
                }
            }
            return true;
        }
    }

    return false;
}

void HandshakeHandler::storeLoginTicket(Session::Ptr session) {
    if (session->accountId == 0 || session->sessionToken.empty()) return;
    // one row per account the client can only ever hold the last blob head it was sent
    Database::instance().executePrepared(
        "REPLACE INTO login_ticket (account_id, token, character_id, issued_at) "
        "VALUES (?, ?, ?, NOW())",
        {static_cast<int32_t>(session->accountId), session->sessionToken,
         static_cast<int32_t>(session->characterId)});
}

// C2S 0x00A7 cstr version then u32 stage then the wide token then the u32 account of the blob head
void HandshakeHandler::handleReauth(Session::Ptr session, Packet& packet) {
    const std::string version = packet.readString(32);
    const int32_t stage = packet.readInt32();
    const std::u16string wide = packet.readWString(64);
    const uint32_t accountId = packet.readUInt32();

    std::string token;
    token.reserve(wide.size());
    for (char16_t c : wide) token.push_back(static_cast<char>(c & 0xFF));

    LOG_INFO("HANDSHAKE", "reauth stage " + std::to_string(stage) + " account " +
             std::to_string(accountId) + " version " + version + " from " +
             session->remoteAddress());

    if (accountId == 0 || token.empty()) {
        LOG_WARN("HANDSHAKE", "reauth with no ticket from " + session->remoteAddress());
        sendDisplayMessage(session, u"MSG_REINPUT_IDPASS", 2);
        session->closeAfterSend();
        return;
    }

    auto rows = Database::instance().queryPrepared(
        "SELECT t.character_id AS character_id, a.username AS username "
        "FROM login_ticket t JOIN accounts a ON a.id = t.account_id "
        "WHERE t.account_id = ? AND t.token = ? "
        "AND t.issued_at > NOW() - INTERVAL 1 DAY LIMIT 1",
        {static_cast<int32_t>(accountId), token});
    if (rows.empty()) {
        // the ticket is gone so the client has to type the credentials again
        LOG_WARN("HANDSHAKE", "reauth ticket unknown for account " + std::to_string(accountId));
        sendDisplayMessage(session, u"MSG_REINPUT_IDPASS", 2);
        session->closeAfterSend();
        return;
    }

    session->accountId        = accountId;
    session->sessionToken     = token;
    session->authenticatedUser = rows[0].at("username");

    // no duplicate login guard here the same account is coming back from its own game socket
    sendSessionInfo(session, CMD::S_SESSION_CONFIRM);
    sendDisplayMessage(session, u"", 1);
    sendServerList(session);
    session->handshakeState = Session::HandshakeState::ChannelListSent;
    // the 0x000E list is what runs FUN 00404410 stage 4 the 0xA7 only refreshes the profile global
    LOG_INFO("HANDSHAKE", "reauth ok account " + std::to_string(accountId) +
             " char " + rows[0].at("character_id") + " channel list sent");
}

void HandshakeHandler::sendSessionInfo(Session::Ptr session, uint16_t opcode) {
    // 0x07 sends DriverID 4 bytes then PlayerInfo 1224 bytes reauth twin 0xA7 same bytes but no launcher notify
    Packet pkt(opcode);

    std::vector<uint8_t> playerInfo(ProfileBlob::kSize, 0);
    ProfileBlob::putI32(playerInfo, ProfileBlob::kSelChar, -1);
    ProfileBlob::putI32(playerInfo, ProfileBlob::kSelKart, -1);

    // the blob head returns on 0xA7 read by sub 480500 so server matches sessions by account and token
    if (session->sessionToken.empty()) session->sessionToken = m_authHandler.generateToken();
    ProfileBlob::writeTicket(playerInfo, session->accountId, session->sessionToken);
    // the same pair must still resolve when the client comes back on 0xA7 after a channel return
    storeLoginTicket(session);

    auto& db = Database::instance();
    
    LOG_DEBUG("HANDSHAKE", "Checking character for account_id=" + std::to_string(session->accountId));
    
    auto results = db.queryPrepared(
        "SELECT id, name, level, experience, gold, cash FROM characters WHERE account_id = ? LIMIT 1",
        {std::to_string(session->accountId)});
    
    if (results.empty()) {
        // no character but 0x07 and 0x02 must still be sent or the client stays invalid

        LOG_INFO("HANDSHAKE", "No character for account " + std::to_string(session->accountId) + " - sending session info with an empty PlayerInfo");

        pkt.writeInt32(session->accountId);
        pkt.writeBytes(playerInfo.data(), playerInfo.size());
        session->send(pkt);
        LOG_DEBUG("HANDSHAKE", "Sent SESSION_INFO (0x07) with empty PlayerInfo");

        // never sends 0x03 here since sub 473730 divides by a catalogue count never sent
        return;
    }
    
    auto& row = results[0];
    int charId = std::stoi(row["id"]);
    session->characterId = static_cast<uint32_t>(charId);   // the redirect ticket carries it
    storeLoginTicket(session);
    std::string nickname = row["name"];
    int level = std::stoi(row["level"]);
    int xp = std::stoi(row["experience"]);
    int gold = std::stoi(row["gold"]);
    int cash = std::stoi(row["cash"]);
    
    LOG_INFO("HANDSHAKE", "Character found: " + nickname + " (ID: " + std::to_string(charId) + 
             ", Level " + std::to_string(level) + ", Gold " + std::to_string(gold) + ")");
    
    ProfileBlob::putNarrowAsWide(playerInfo, ProfileBlob::kNickname, nickname, ProfileBlob::kNickMax);
    // exp at 0x4A4 and gold at 0x4AC the old writer put gold where the client reads exp
    ProfileBlob::writeWallet(playerInfo, level, xp, cash, gold);

    // 0x4B0 and 0x4B4 are preview instance ids character then kart order a capture proved it
    int32_t selKart = -1;
    int32_t selChar = -1;
    {
        auto k = db.queryPrepared(
            "SELECT id FROM owned_kart WHERE character_id = ? AND active_flag = 1 ORDER BY id LIMIT 1",
            {std::to_string(charId)});
        if (!k.empty()) selKart = std::stoi(k[0].at("id"));
        auto c = db.queryPrepared(
            "SELECT id FROM owned_character WHERE character_id = ? AND active_flag = 1 ORDER BY id LIMIT 1",
            {std::to_string(charId)});
        if (!c.empty()) selChar = std::stoi(c[0].at("id"));
    }
    ProfileBlob::putI32(playerInfo, ProfileBlob::kSelChar, selChar);
    ProfileBlob::putI32(playerInfo, ProfileBlob::kSelKart, selKart);
    LOG_INFO("HANDSHAKE", "lobby selection kart inst " + std::to_string(selKart) +
             " char inst " + std::to_string(selChar) + " for char " + std::to_string(charId));
    
    pkt.writeInt32(charId);
    pkt.writeBytes(playerInfo.data(), playerInfo.size());
    
    session->send(pkt);
    LOG_DEBUG("HANDSHAKE", "Sent SESSION_INFO (0x07) with DriverID=" + std::to_string(charId));
}

void HandshakeHandler::sendConnectionOK(Session::Ptr session) {
    Packet pkt(CMD::S_CONNECTION_OK);
    
    // 38 bytes of connection data holding a server timestamp or session info and configuration flags
    for (int i = 0; i < 38; ++i) {
        pkt.writeUInt8(0);
    }
    
    session->send(pkt);
    LOG_DEBUG("HANDSHAKE", "Sent CONNECTION_OK (0x0A)");
}

void HandshakeHandler::sendDisplayMessage(Session::Ptr session, 
                                           const std::u16string& message, int32_t code) {
    // cmd 0x02 code one gates UI else disconnect
    Packet pkt(CMD::S_DISPLAY_MESSAGE);
    
    for (char16_t c : message) {
        pkt.writeUInt8(static_cast<uint8_t>(c & 0xFF));
        pkt.writeUInt8(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    
    pkt.writeInt32(code);
    
    session->send(pkt);
    LOG_DEBUG("HANDSHAKE", "Sent DISPLAY_MESSAGE (0x02) with code=" + std::to_string(code));
}

void HandshakeHandler::sendServerRedirect(Session::Ptr session, 
                                          const std::string& ip, int port,
                                          const std::string& token) {
    // STORE session in DB for GameServer to retrieve
    auto& db = Database::instance();
    
    db.executePrepared("DELETE FROM active_sessions WHERE account_id = ?",
                       {std::to_string(session->accountId)});
    
    // Insert new session - GameServer will look this up by IP or token
    std::string clientIp = session->remoteAddress();
    // strips the port suffix off a client ip address before use
    size_t colonPos = clientIp.find(':');
    if (colonPos != std::string::npos) {
        clientIp = clientIp.substr(0, colonPos);
    }
    
    if (db.executePrepared(
            "INSERT INTO active_sessions (account_id, character_id, token, client_ip, created_at) VALUES (?, ?, ?, ?, NOW())",
            {std::to_string(session->accountId), std::to_string(session->characterId), token, clientIp})) {
        LOG_INFO("HANDSHAKE", "Stored session for account " + std::to_string(session->accountId) + 
                 " char " + std::to_string(session->characterId) + " IP=" + clientIp);
    } else {
        LOG_ERROR("HANDSHAKE", "Failed to store session in DB!");
    }
    
    // sub 47AA00 sub 4774C0 arm two recvs mid parse and wedge the client sub 479340 defers safely
    const char* alt = std::getenv("KNC_REDIRECT_0X19");
    const bool useLegacy = !(alt && alt[0] == '1');

    Packet pkt(useLegacy ? static_cast<uint16_t>(CMD::S_SERVER_REDIRECT)
                         : static_cast<uint16_t>(0x19));
    if (useLegacy) {
        pkt.writeInt32(0);
        pkt.writeString(ip);
        pkt.writeInt32(port);
    } else {
        pkt.writeString(ip);
        pkt.writeInt32(port);
        pkt.writeInt32(8);
    }
    
    auto data = pkt.serialize();
    std::string hexDump = std::string(useLegacy ? "0x54" : "0x19") + " payload: ";
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 8; i < data.size(); i++) {  // Skip header
        hexDump += hex[data[i] >> 4];
        hexDump += hex[data[i] & 0xF];
        hexDump += ' ';
    }
    LOG_DEBUG("HANDSHAKE", hexDump);
    
    session->send(pkt);
    LOG_INFO("HANDSHAKE", std::string("Redirecting via ") + (useLegacy ? "0x54" : "0x19") +
             " to " + ip + ":" + std::to_string(port));
}

} // namespace knc
