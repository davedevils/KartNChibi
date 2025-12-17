/**
 * @file GameServer.cpp
 * @brief GameServer implementation - handles all game packets
 */

#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "logging/Logger.h"
#include "security/BanManager.h"
#include "db/Database.h"
#include "handlers/LicenseHandler.h"
#include "handlers/MissionHandler.h"
#include "handlers/AntiCheatHandler.h"
#include "handlers/GarageHandler.h"
#include "handlers/LobbyHandler.h"
#include "handlers/GhostHandler.h"
#include "handlers/ScenarioHandler.h"

namespace knc {

GameServer::GameServer(int port)
    : m_acceptor(m_ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<uint16_t>(port)))
{
    startAccept();
}

void GameServer::run() {
    LOG_INFO("GAME", "Server running...");
    m_ioContext.run();
}

void GameServer::stop() {
    m_ioContext.stop();
    LOG_INFO("GAME", "Server stopped");
}

void GameServer::startAccept() {
    m_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            std::string ip = socket.remote_endpoint().address().to_string();
            
            if (BanManager::instance().isBanned(ip)) {
                LOG_WARN("GAME", "Rejected banned IP: " + ip);
                socket.close();
            } else {
                auto session = std::make_shared<Session>(std::move(socket));
                LOG_INFO("GAME", "New connection from " + session->remoteAddress() + " (ID: " + std::to_string(session->id()) + ")");
                
                session->setPacketHandler([this](Session::Ptr s, Packet& pkt) {
                    handlePacket(s, pkt);
                });
                
                session->setDisconnectHandler([this](Session::Ptr s) {
                    onDisconnect(s);
                });
                
                addSession(session);
                session->start();
                
                // Look up session from DB (stored by LoginServer during redirect)
                auto& db = Database::instance();
                auto sessions = db.query(
                    "SELECT account_id, character_id, token FROM active_sessions WHERE client_ip = '" +
                    db.escapeString(ip) + "' ORDER BY created_at DESC LIMIT 1"
                );
                
                if (!sessions.empty()) {
                    session->accountId = std::stoi(sessions[0]["account_id"]);
                    session->characterId = std::stoi(sessions[0]["character_id"]);
                    session->sessionToken = sessions[0]["token"];
                    
                    LOG_INFO("GAME", "Found pending session for IP " + ip + 
                             ": account=" + std::to_string(session->accountId) +
                             " char=" + std::to_string(session->characterId));
                    
                    // Client after redirect checks byte_8CCDC0 flag set by receiving 0xA7
                    // We MUST send 0xA7 IMMEDIATELY before client's 1000ms timeout
                    
                    // Load character data
                    auto chars = db.queryPrepared(
                        "SELECT name, level, experience, gold, cash, wins, losses FROM characters WHERE account_id = ? LIMIT 1",
                        {std::to_string(session->accountId)}
                    );
                    
                    if (!chars.empty()) {
                        PlayerData player;
                        player.name = chars[0]["name"];
                        player.level = std::stoi(chars[0]["level"]);
                        player.xp = std::stoi(chars[0]["experience"]);
                        player.gold = std::stoi(chars[0]["gold"]);
                        player.cash = std::stoi(chars[0]["cash"]);
                        player.wins = std::stoi(chars[0]["wins"]);
                        player.losses = std::stoi(chars[0]["losses"]);
                        player.driverId = 1;
                        
                        // Send 0xA7 IMMEDIATELY - sets client's byte_8CCDC0 flag
                        session->send(PacketBuilder::sessionConfirm(session->accountId, player));
                        LOG_INFO("GAME", "Sent 0xA7 SESSION_CONFIRM");
                        
                        // Also send 0x12 (SHOW_LOBBY) immediately
                        Packet showLobby(CMD::S_SHOW_LOBBY);
                        session->send(showLobby);
                        LOG_INFO("GAME", "Sent 0x12 SHOW_LOBBY");
                        
                        // And room list (0x3F)
                        Packet roomList(CMD::S_ROOM_INFO);
                        roomList.writeInt32(0);
                        session->send(roomList);
                        LOG_INFO("GAME", "Sent 0x3F ROOM_LIST count=0");
                    }
                    
                    // Delete the pending session from DB (one-time use)
                    db.execute("DELETE FROM active_sessions WHERE client_ip = '" + 
                               db.escapeString(ip) + "'");
                } else {
                    LOG_WARN("GAME", "No pending session found for IP " + ip + " - sending basic init");
                    // Send basic init packets - client might be connecting directly
                    session->send(PacketBuilder::connectionOk());
                    session->send(PacketBuilder::displayMessage(u"", 1));
                }
                
                LOG_INFO("GAME", "Client initialized: " + session->remoteAddress());
            }
        }
        
        startAccept();
    });
}

static std::string toHex(uint8_t v) {
    const char* hex = "0123456789ABCDEF";
    return std::string(1, hex[v >> 4]) + hex[v & 0xF];
}

void GameServer::handlePacket(Session::Ptr session, Packet& packet) {
    uint8_t cmd = packet.cmd();
    
    // log received packet
    std::string hexDump;
    const auto& payload = packet.payload();
    for (size_t i = 0; i < std::min<size_t>(32, payload.size()); ++i) {
        if (i > 0) hexDump += " ";
        hexDump += toHex(payload[i]);
    }
    if (payload.size() > 32) hexDump += "...";
    
    LOG_DEBUG("GAME", "RECV from " + session->remoteAddress() + ": CMD=0x" + toHex(cmd) + 
             " Flag=0x" + toHex(packet.flag()) + " Size=" + std::to_string(packet.payloadSize()) +
             " Data=[" + hexDump + "]");
    
    switch (cmd) {
        // ===== AUTH =====
        case CMD::C_HEARTBEAT:      handleHeartbeat(session, packet); break;
        case CMD::C_CLIENT_AUTH:    handleClientAuth(session, packet); break;
        case CMD::C_FULL_STATE:     handleFullState(session, packet); break;
        case CMD::C_CLIENT_INFO:    handleClientInfo(session, packet); break;
        case CMD::S_SESSION_CONFIRM: handleSessionConfirm(session, packet); break;
        
        // ===== LOBBY =====
        case CMD::C_SERVER_QUERY:   handleServerQuery(session, packet); break;
        
        // ===== ROOM =====
        case CMD::C_CREATE_ROOM:    handleCreateRoom(session, packet); break;  // 0x63
        case CMD::C_JOIN_ROOM:      handleJoinRoom(session, packet); break;    // 0x3F
        case CMD::C_LEAVE_ROOM:     handleLeaveRoom(session, packet); break;   // 0x22
        case CMD::C_ROOM_STATE_REQ: handleRoomState(session, packet); break;   // 0x30
        case CMD::C_PLAYER_READY:   handlePlayerReady(session, packet); break; // 0x23
        case CMD::C_GAME_START:     handleGameStart(session, packet); break;   // 0x40
        
        // ===== CHAT =====
        case CMD::C_CHAT_MESSAGE:   handleChatMessage(session, packet); break;
        case CMD::C_WHISPER:        handleWhisper(session, packet); break;
        case CMD::C_LOBBY_CHAT:     handleLobbyChat(session, packet); break;     // 0xB4
        
        // ===== GAME/RACE =====
        case CMD::C_STATE_CHANGE:   handleStateChange(session, packet); break;
        case CMD::C_POSITION:       handlePosition(session, packet); break;
        case CMD::C_LAP_COMPLETE:   handleLapComplete(session, packet); break;
        case CMD::C_ITEM_PICKUP:    handleItemPickup(session, packet); break;
        case CMD::C_ITEM_HIT:       handleItemHit(session, packet); break;
        case CMD::C_RACE_FINISH:    handleRaceFinish(session, packet); break;
        case CMD::C_ITEM_USE:       handleItemUse(session, packet); break;
        
        // ===== SHOP =====
        case CMD::C_SHOP_ENTER:     m_shopHandler.handleEnterShop(session, this); break;
        case CMD::C_SHOP_EXIT:      m_shopHandler.handleExitShop(session, this); break;
        case CMD::C_PURCHASE:       m_shopHandler.handlePurchase(session, packet, this); break;
        case CMD::C_SHOP_BROWSE:    handleShopBrowse(session, packet); break;
        case CMD::C_SELL_ITEM:      handleSellItem(session, packet); break;
        
        // ===== INVENTORY =====
        case CMD::C_EQUIP_VEHICLE:    handleEquipVehicle(session, packet); break;
        case CMD::C_EQUIP_ACCESSORY:  handleEquipAccessory(session, packet); break;
        case CMD::C_USE_ITEM:         handleUseItem(session, packet); break;
        
        // ===== TUTORIAL/LICENSE =====
        case CMD::C_START_TUTORIAL:   LicenseHandler::handleStartTutorial(session, packet, this); break;
        case CMD::C_TUTORIAL_COMPLETE: LicenseHandler::handleTutorialComplete(session, packet, this); break;
        case CMD::C_LICENSE_TEST:     LicenseHandler::handleLicenseTest(session, packet, this); break;
        case CMD::C_LICENSE_RESULT:   LicenseHandler::handleLicenseResult(session, packet, this); break;
        
        // ===== MISSION/QUEST =====
        case CMD::C_MISSION_LIST:     MissionHandler::handleGetMissionList(session, packet, this); break;
        case CMD::C_MISSION_DETAILS:  MissionHandler::handleGetMissionDetails(session, packet, this); break;
        case CMD::C_CLAIM_REWARD:     MissionHandler::handleClaimReward(session, packet, this); break;
        
        // ===== GARAGE =====
        case CMD::C_OPEN_GARAGE:      GarageHandler::handleOpenGarage(session, packet, this); break;
        case CMD::C_GARAGE_VEHICLES:  GarageHandler::handleGetVehicleList(session, this); break;
        case CMD::C_GARAGE_ITEMS:     GarageHandler::handleGetItemList(session, this); break;
        case CMD::C_GARAGE_ACCESSORIES: GarageHandler::handleGetAccessoryList(session, this); break;
        case CMD::C_UPGRADE_VEHICLE:  GarageHandler::handleUpgradeVehicle(session, packet, this); break;
        case CMD::C_REPAIR_VEHICLE:   GarageHandler::handleRepairVehicle(session, packet, this); break;
        case CMD::C_DELETE_ITEM:      GarageHandler::handleDeleteItem(session, packet, this); break;
        
        // ===== QUICK MATCH =====
        case CMD::C_QUICK_MATCH:      LobbyHandler::handleQuickMatch(session, packet, this); break;
        
        // ===== FRIENDS =====
        case CMD::C_ADD_FRIEND:       LobbyHandler::handleAddFriend(session, packet, this); break;
        case CMD::C_REMOVE_FRIEND:    LobbyHandler::handleRemoveFriend(session, packet, this); break;
        case CMD::C_BLOCK_PLAYER:     LobbyHandler::handleBlockPlayer(session, packet, this); break;
        case CMD::C_PLAYER_PROFILE:   LobbyHandler::handlePlayerProfile(session, packet, this); break;
        
        // ===== DRIFT / MINI TURBO =====
        case CMD::C_DRIFT_START: {
            auto room = getRoom(session->roomId);
            if (room) m_raceHandler.handleDriftStart(session, packet, room.get());
            break;
        }
        case CMD::C_DRIFT_END: {
            auto room = getRoom(session->roomId);
            if (room) m_raceHandler.handleDriftEnd(session, packet, room.get());
            break;
        }
        case CMD::C_BOOST_ACTIVATE: {
            auto room = getRoom(session->roomId);
            if (room) m_raceHandler.handleBoostActivate(session, packet, room.get());
            break;
        }
        case CMD::C_BOOST_END: {
            auto room = getRoom(session->roomId);
            if (room) m_raceHandler.handleBoostEnd(session, room.get());
            break;
        }
        
        // ===== GHOST MODE =====
        case CMD::C_GHOST_MENU:       GhostHandler::handleOpenGhostMenu(session, this); break;
        case CMD::C_GHOST_SELECT_MAP: GhostHandler::handleSelectMap(session, packet, this); break;
        case CMD::C_GHOST_START:      GhostHandler::handleStartGhostRace(session, packet, this); break;
        case CMD::C_GHOST_COMPLETE:   GhostHandler::handleGhostRaceComplete(session, packet, this); break;
        case CMD::C_GHOST_SAVE:       GhostHandler::handleSaveGhost(session, packet, this); break;
        case CMD::C_GHOST_LIST:       GhostHandler::handleGetGhostList(session, packet, this); break;
        case CMD::C_GHOST_DOWNLOAD:   GhostHandler::handleDownloadGhost(session, packet, this); break;
        
        // ===== SCENARIO MODE =====
        case CMD::C_SCENARIO_MENU:     ScenarioHandler::handleOpenScenarioMenu(session, this); break;
        case CMD::C_SCENARIO_CHAPTER:  ScenarioHandler::handleSelectChapter(session, packet, this); break;
        case CMD::C_SCENARIO_STAGE:    ScenarioHandler::handleSelectStage(session, packet, this); break;
        case CMD::C_SCENARIO_START:    ScenarioHandler::handleStartScenario(session, packet, this); break;
        case CMD::C_SCENARIO_COMPLETE: ScenarioHandler::handleScenarioComplete(session, packet, this); break;
        case CMD::C_SCENARIO_PROGRESS: ScenarioHandler::handleGetProgress(session, this); break;
        case CMD::C_SCENARIO_CHAPTERS: ScenarioHandler::handleGetChapterList(session, this); break;
        
        // ===== DATA REQUESTS =====
        case CMD::C_REQUEST_DATA:  // 0x4D - 276 byte data request
            handleRequestData(session, packet);
            break;
        case CMD::C_UNKNOWN_32:  // 0x32 - Unknown data (8 bytes)
            handleUnknown32(session, packet);
            break;
        
        // ===== CHANNEL SELECT (from redirect) =====
        case CMD::C_CHANNEL_SELECT:  // 0x18 - client sends after redirect
            handleChannelSelect(session, packet);
            break;
        
        // ===== ACK/MISC =====
        case CMD::C_ACK:  // 0x8E - just acknowledge
            break;
        case 0x0B:  // ACK reply - ignore
            break;
        
        // ===== DISCONNECT =====
        case CMD::C_DISCONNECT:
            LOG_INFO("GAME", "Client requested disconnect: " + session->remoteAddress());
            session->stop();
            break;
        
        default:
            LOG_DEBUG("GAME", "Unhandled CMD: 0x" + toHex(cmd));
            break;
    }
}

// =============================================================================
// AUTH HANDLERS
// =============================================================================

void GameServer::handleHeartbeat(Session::Ptr session, Packet& packet) {
    (void)packet;
    (void)session;
    // do NOT respond - 0x12 would trigger lobby transition!
    // heartbeat is just keep-alive, no response needed
}

void GameServer::handleClientAuth(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_INFO("GAME", "Client auth from " + session->remoteAddress());
    sendPlayerData(session);
}

void GameServer::handleFullState(Session::Ptr session, Packet& packet) {
    (void)packet;
    session->send(PacketBuilder::initResponse());
}

void GameServer::handleClientInfo(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_INFO("GAME", "Client info from " + session->remoteAddress());
    session->send(PacketBuilder::ack());
}

void GameServer::handleSessionConfirm(Session::Ptr session, Packet& packet) {
    // Client sends 0xA7 after redirect - this is the first packet from client
    // Format: [version:4][state:4][charName:wstring][driverID:4]
    
    if (packet.payloadSize() < 10) {
        return;
    }
    
    std::string version = packet.readString(4);
    int32_t state = packet.readInt32();
    
    LOG_INFO("GAME", "Session confirm from " + session->remoteAddress() + 
             " state=" + std::to_string(state) + " accountId=" + std::to_string(session->accountId));
    
    // Client sent 0xA7 after receiving our 0xA7 - now send lobby
    if (session->accountId > 0) {
        // Send SHOW_LOBBY (0x12)
        Packet showLobby(CMD::S_SHOW_LOBBY);
        session->send(showLobby);
        LOG_INFO("GAME", "Sent 0x12 SHOW_LOBBY");
        
        // Send room list (0x3F)
        Packet roomList(CMD::S_ROOM_INFO);
        {
            std::lock_guard<std::mutex> lock(m_roomsMutex);
            roomList.writeInt32(static_cast<int32_t>(m_rooms.size()));
        }
        session->send(roomList);
        LOG_INFO("GAME", "Sent 0x3F ROOM_LIST count=0");
    } else {
        LOG_WARN("GAME", "No valid session - sending full player data");
        sendPlayerData(session);
    }
}

// =============================================================================
// CHANNEL SELECT (after redirect from LoginServer)
// =============================================================================

void GameServer::handleChannelSelect(Session::Ptr session, Packet& packet) {
    // Client sends 0x18 after redirect - same format as to LoginServer
    // Format: [screen:int32][channelId:int32]
    
    int32_t screen = packet.readInt32();
    int32_t channelId = packet.readInt32();
    
    LOG_INFO("GAME", "Channel select from " + session->remoteAddress() + 
             " screen=0x" + toHex(screen) + " channel=" + std::to_string(channelId));
    
    // Respond with SHOW_LOBBY (0x12) to enter the lobby
    Packet showLobby(CMD::S_SHOW_LOBBY);
    session->send(showLobby);
    LOG_INFO("GAME", "Sent 0x12 SHOW_LOBBY in response to channel select");
    
    // Send room list (0x3F)
    Packet roomList(CMD::S_ROOM_INFO);
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        roomList.writeInt32(static_cast<int32_t>(m_rooms.size()));
    }
    session->send(roomList);
    LOG_INFO("GAME", "Sent 0x3F ROOM_LIST");
}

// =============================================================================
// LOBBY HANDLERS
// =============================================================================

void GameServer::handleLobbyRequest(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_INFO("GAME", "Lobby request from " + session->remoteAddress());
    
    // send room list (thread-safe)
    std::vector<RoomData> roomList;
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (const auto& [id, room] : m_rooms) {
            RoomData rd;
            rd.id = id;
            rd.name = room->name();
            rd.currentPlayers = static_cast<uint8_t>(room->playerCount());
            rd.maxPlayers = room->settings().maxPlayers;
            rd.mode = static_cast<uint8_t>(room->settings().mode);
            rd.mapId = room->settings().mapId;
            rd.state = static_cast<uint8_t>(room->state());
            rd.isPrivate = room->settings().isPrivate;
            roomList.push_back(rd);
        }
    }
    
    session->send(PacketBuilder::showLobby(roomList));
}

void GameServer::handleServerQuery(Session::Ptr session, Packet& packet) {
    (void)packet;
    // respond with server info
    session->send(PacketBuilder::ack());
}

// =============================================================================
// ROOM HANDLERS
// =============================================================================

void GameServer::handleCreateRoom(Session::Ptr session, Packet& packet) {
    RoomSettings settings;
    settings.name = packet.readString(32);
    settings.password = packet.readString(16);
    settings.mode = static_cast<GameMode>(packet.readUInt8());
    settings.maxPlayers = packet.readUInt8();
    settings.mapId = packet.readUInt8();
    settings.laps = packet.readUInt8();
    settings.isPrivate = !settings.password.empty();
    
    auto room = createRoom(settings);
    
    if (room) {
        room->addPlayer(session);
        session->roomId = room->id();
        
        session->send(PacketBuilder::createRoomResponse(room->id(), true));
        
        RoomData rd;
        rd.id = room->id();
        rd.name = settings.name;
        rd.mode = static_cast<uint8_t>(settings.mode);
        rd.maxPlayers = settings.maxPlayers;
        rd.mapId = settings.mapId;
        rd.laps = settings.laps;
        session->send(PacketBuilder::roomInfo(rd));
        
        LOG_INFO("ROOM", "Room created: " + settings.name + " by " + session->remoteAddress());
    } else {
        session->send(PacketBuilder::createRoomResponse(0, false));
    }
}

void GameServer::handleJoinRoom(Session::Ptr session, Packet& packet) {
    uint32_t roomId = packet.readUInt32();
    std::string password = packet.readString(16);
    
    auto room = getRoom(roomId);
    
    if (!room) {
        LOG_WARN("ROOM", "Room not found: " + std::to_string(roomId));
        session->send(PacketBuilder::displayMessage(u"Room not found", 0));
        return;
    }
    
    // Check if game already started
    if (room->state() != RoomState::Waiting) {
        session->send(PacketBuilder::displayMessage(u"Game in progress", 0));
        return;
    }
    
    if (room->isFull()) {
        // 0x21 is NOT "room full" message! Use displayMessage instead
        session->send(PacketBuilder::displayMessage(u"Room is full", 0));
        return;
    }
    
    if (room->settings().isPrivate && room->settings().password != password) {
        session->send(PacketBuilder::displayMessage(u"Wrong password", 0));
        return;
    }
    
    // Check if already in a room
    if (session->roomId != 0) {
        auto oldRoom = getRoom(session->roomId);
        if (oldRoom) {
            oldRoom->removePlayer(session->id());
        }
    }
    
    // Get player's equipped vehicle
    auto& db = Database::instance();
    int32_t vehicleTemplateId = 1;  // Default
    auto vehResult = db.query(
        "SELECT vehicle_type_id FROM vehicles WHERE character_id = " + 
        std::to_string(session->characterId) + " AND equipped = 1 LIMIT 1"
    );
    if (!vehResult.empty()) {
        vehicleTemplateId = std::stoi(vehResult[0]["vehicle_type_id"]);
    }
    
    // Add player to room
    if (room->addPlayer(session, session->characterId, session->characterName, vehicleTemplateId)) {
        session->roomId = room->id();
        
        // Build PlayerData for join packet
        auto* roomPlayer = room->getPlayer(session->id());
        PlayerData pd;
        pd.id = session->characterId;
        for (char16_t c : session->characterName) {
            if (c > 0 && c < 128) pd.name += static_cast<char>(c);
        }
        pd.slot = roomPlayer ? roomPlayer->slot : 0;
        pd.vehicleTemplateId = vehicleTemplateId;
        pd.ready = false;
        
        // Notify other players in room
        room->broadcastExcept(PacketBuilder::playerJoin(pd), session->id());
        
        // Send room info to joining player
        RoomData rd;
        rd.id = room->id();
        rd.name = room->name();
        rd.mode = static_cast<uint8_t>(room->settings().mode);
        rd.maxPlayers = room->settings().maxPlayers;
        rd.mapId = room->settings().mapId;
        rd.laps = room->settings().laps;
        rd.currentPlayers = static_cast<uint8_t>(room->playerCount());
        session->send(PacketBuilder::roomInfo(rd));
        
        // Send info about all existing players to the joining player
        for (const auto& existingPlayer : room->getPlayers()) {
            if (existingPlayer.sessionId != session->id()) {
                PlayerData epd;
                epd.id = existingPlayer.characterId;
                for (char16_t c : existingPlayer.name) {
                    if (c > 0 && c < 128) epd.name += static_cast<char>(c);
                }
                epd.slot = existingPlayer.slot;
                epd.vehicleTemplateId = existingPlayer.vehicleTemplateId;
                epd.ready = existingPlayer.ready;
                session->send(PacketBuilder::playerJoin(epd));
            }
        }
        
        LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) + 
                 " joined room " + std::to_string(roomId) + 
                 " (total: " + std::to_string(room->playerCount()) + ")");
    }
}

void GameServer::handleLeaveRoom(Session::Ptr session, Packet& packet) {
    (void)packet;
    
    auto room = getRoom(session->roomId);
    if (room) {
        bool wasHost = room->isHost(session->id());
        uint32_t oldHostSession = room->hostSessionId();
        
        room->removePlayer(session->id());
        
        if (!room->isEmpty()) {
            // Notify others that player left
            room->broadcast(PacketBuilder::playerLeft(session->characterId));
            
            // If host changed, notify all players
            if (wasHost && room->hostSessionId() != oldHostSession) {
                auto* newHost = room->getPlayer(room->hostSessionId());
                if (newHost) {
                    // Send player update to indicate new host
                    PlayerData hostData;
                    hostData.id = newHost->characterId;
                    for (char16_t c : newHost->name) {
                        if (c > 0 && c < 128) hostData.name += static_cast<char>(c);
                    }
                    hostData.slot = newHost->slot;
                    room->broadcast(PacketBuilder::playerUpdate(hostData));
                    LOG_INFO("ROOM", "Host transferred to char " + std::to_string(newHost->characterId));
                }
            }
        } else {
            // Room is empty, remove it
            LOG_INFO("ROOM", "Room " + std::to_string(room->id()) + " is now empty, removing");
            removeRoom(room->id());
        }
    }
    
    session->roomId = 0;
    session->send(PacketBuilder::playerDisconnect(session->characterId));
    session->send(PacketBuilder::showLobby({}));
    
    LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) + " left room");
}

void GameServer::handleRoomState(Session::Ptr session, Packet& packet) {
    if (packet.flag() == 0x01) {
        session->send(PacketBuilder::ack());
    }
}

// =============================================================================
// CHAT HANDLERS
// =============================================================================

void GameServer::handleChatMessage(Session::Ptr session, Packet& packet) {
    // 0x2D can be chat OR room creation depending on context
    // If not in a room and payload > 40 bytes, it's likely room creation
    
    if (session->roomId == 0 && packet.payloadSize() > 40) {
        // Room creation: wstring roomName, wstring password, uint8 mode, uint8 maxPlayers, uint8 mapId, uint8 laps
        std::u16string roomNameW = packet.readWString();
        std::u16string passwordW = packet.readWString();
        
        // convert to string for logging
        std::string roomName, password;
        for (char16_t c : roomNameW) if (c > 0 && c < 128) roomName += static_cast<char>(c);
        for (char16_t c : passwordW) if (c > 0 && c < 128) password += static_cast<char>(c);
        
        // read room settings
        uint8_t mode = packet.payloadSize() > packet.readPos() ? packet.readUInt8() : 0;
        uint8_t maxPlayers = packet.payloadSize() > packet.readPos() ? packet.readUInt8() : 8;
        uint8_t mapId = packet.payloadSize() > packet.readPos() ? packet.readUInt8() : 0;
        uint8_t laps = packet.payloadSize() > packet.readPos() ? packet.readUInt8() : 3;
        
        LOG_INFO("ROOM", "Create room request: '" + roomName + "' password='" + password + 
                 "' mode=" + std::to_string(mode) + " max=" + std::to_string(maxPlayers) +
                 " map=" + std::to_string(mapId) + " laps=" + std::to_string(laps));
        
        // create room
        RoomSettings settings;
        settings.name = roomName;
        settings.password = password;
        settings.mode = static_cast<GameMode>(mode);
        settings.maxPlayers = maxPlayers > 0 ? maxPlayers : 8;
        settings.mapId = mapId;
        settings.laps = laps > 0 ? laps : 3;
        settings.isPrivate = !password.empty();
        
        auto room = createRoom(settings);
        if (room) {
            room->addPlayer(session);
            session->roomId = room->id();
            
            // send room created response (0x63)
            session->send(PacketBuilder::createRoomResponse(room->id(), true));
            
            // send room info (0x3F)
            RoomData rd;
            rd.id = room->id();
            rd.name = settings.name;
            rd.mode = static_cast<uint8_t>(settings.mode);
            rd.maxPlayers = settings.maxPlayers;
            rd.mapId = settings.mapId;
            rd.laps = settings.laps;
            session->send(PacketBuilder::roomInfo(rd));
            
            LOG_INFO("ROOM", "Room created: ID=" + std::to_string(room->id()) + " '" + roomName + "'");
        } else {
            session->send(PacketBuilder::createRoomResponse(0, false));
            LOG_WARN("ROOM", "Failed to create room");
        }
        return;
    }
    
    // normal chat in room
    std::u16string msg = packet.readWString();
    auto room = getRoom(session->roomId);
    if (room) {
        room->broadcast(PacketBuilder::chatMessage("Player", msg));
    }
    
    LOG_DEBUG("CHAT", "Chat from " + session->remoteAddress());
}

void GameServer::handleWhisper(Session::Ptr session, Packet& packet) {
    std::u16string targetName = packet.readWString();
    std::u16string msg = packet.readWString();
    
    // Find target session by character name
    Session::Ptr targetSession = nullptr;
    for (auto& [id, sess] : m_sessions) {
        if (sess && sess->characterName == targetName) {
            targetSession = sess;
            break;
        }
    }
    
    if (targetSession) {
        // Convert sender name for chat
        std::string senderStr;
        for (char16_t c : session->characterName) {
            if (c > 0 && c < 128) senderStr += static_cast<char>(c);
        }
        
        // Send whisper to target
        targetSession->send(PacketBuilder::chatMessage(senderStr, msg));
        session->send(PacketBuilder::whisperEnable());
        LOG_INFO("CHAT", "Whisper sent from " + senderStr);
    } else {
        // Target not found
        session->send(PacketBuilder::displayMessage(u"Player not found", 0));
    }
}

void GameServer::handleLobbyChat(Session::Ptr session, Packet& packet) {
    // 0xB4 - Lobby chat
    // client sends: wstring message, int32 type
    std::u16string message = packet.readWString();
    int32_t chatType = 0;
    if (packet.payloadSize() >= 4) {
        chatType = packet.readInt32();
    }
    
    // Convert message to ASCII for logging
    std::string msgStr;
    for (char16_t c : message) {
        if (c > 0 && c < 128) msgStr += static_cast<char>(c);
    }
    
    // use character name from session
    std::u16string senderName = session->characterName.empty() ? u"Player" : session->characterName;
    
    std::string nameStr;
    for (char16_t c : senderName) {
        if (c > 0 && c < 128) nameStr += static_cast<char>(c);
    }
    
    LOG_INFO("CHAT", "Lobby chat from " + nameStr + ": '" + msgStr + "' type=" + std::to_string(chatType));
    
    // check for whisper command: /w [name] [message]
    if (message.size() > 3 && message[0] == u'/' && message[1] == u'w' && message[2] == u' ') {
        // parse: /w targetName message
        size_t nameStart = 3;
        size_t nameEnd = message.find(u' ', nameStart);
        if (nameEnd != std::u16string::npos) {
            std::u16string targetName = message.substr(nameStart, nameEnd - nameStart);
            std::u16string whisperMsg = message.substr(nameEnd + 1);
            
            // find target session
            Session::Ptr targetSession = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_sessionsMutex);
                for (auto& [id, sess] : m_sessions) {
                    if (sess->characterName == targetName) {
                        targetSession = sess;
                        break;
                    }
                }
            }
            
            if (targetSession) {
                // send whisper to target
                Packet whisper(CMD::S_SYSTEM_MESSAGE);
                whisper.writeInt32(session->id());
                whisper.writeWString(senderName);
                whisper.writeWString(whisperMsg);
                whisper.writeInt32(1);  // type 1 = whisper?
                targetSession->send(whisper);
                
                // echo back to sender
                session->send(whisper);
                
                LOG_INFO("CHAT", "Whisper sent to target");
            } else {
                // target not found - send error to sender
                Packet err(CMD::S_SYSTEM_MESSAGE);
                err.writeInt32(0);
                err.writeWString(u"System");
                err.writeWString(u"Player not found");
                err.writeInt32(0);
                session->send(err);
            }
            return;
        }
    }
    
    // normal chat - broadcast to all sessions in lobby
    Packet resp(CMD::S_SYSTEM_MESSAGE);  // 0xB4
    resp.writeInt32(session->id());      // sender id
    resp.writeWString(senderName);       // sender name
    resp.writeWString(message);          // message
    resp.writeInt32(0);                  // type 0 = chat
    
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& [id, sess] : m_sessions) {
        sess->send(resp);
    }
}

// =============================================================================
// GAME HANDLERS
// =============================================================================

void GameServer::handleStateChange(Session::Ptr session, Packet& packet) {
    (void)packet;
    LOG_DEBUG("GAME", "State change from " + session->remoteAddress());
}

void GameServer::handlePosition(Session::Ptr session, Packet& packet) {
    float x = packet.readFloat();
    float y = packet.readFloat();
    float z = packet.readFloat();
    float rot = packet.readFloat();
    
    // Anti-cheat validation
    if (!AntiCheatHandler::validatePosition(session, x, y, z)) {
        LOG_WARN("GAME", "Position validation failed for " + session->remoteAddress());
        return;  // Don't broadcast invalid position
    }
    
    auto room = getRoom(session->roomId);
    if (room) {
        room->broadcastExcept(PacketBuilder::position(session->characterId, x, y, z, rot), session->id());
    }
}

// =============================================================================
// RACE HANDLERS
// =============================================================================

void GameServer::handleLapComplete(Session::Ptr session, Packet& packet) {
    auto room = getRoom(session->roomId);
    if (room) {
        m_raceHandler.handleLapComplete(session, packet, room.get());
    }
}

void GameServer::handleItemPickup(Session::Ptr session, Packet& packet) {
    auto room = getRoom(session->roomId);
    if (room) {
        m_raceHandler.handleItemPickup(session, packet, room.get());
    }
}

void GameServer::handleItemHit(Session::Ptr session, Packet& packet) {
    auto room = getRoom(session->roomId);
    if (room) {
        m_raceHandler.handleItemHit(session, packet, room.get());
    }
}

void GameServer::handleRaceFinish(Session::Ptr session, Packet& packet) {
    auto room = getRoom(session->roomId);
    if (room) {
        m_raceHandler.handleFinish(session, packet, room.get(), this);
    }
}

void GameServer::handleItemUse(Session::Ptr session, Packet& packet) {
    auto room = getRoom(session->roomId);
    if (room) {
        m_raceHandler.handleItemUse(session, packet, room.get());
    }
}

void GameServer::handleGameStart(Session::Ptr session, Packet& packet) {
    (void)packet;
    auto room = getRoom(session->roomId);
    if (!room) {
        LOG_WARN("ROOM", "GameStart: player not in room");
        return;
    }
    
    // Only host can start
    if (!room->isHost(session->id())) {
        session->send(PacketBuilder::displayMessage(u"Only host can start", 0));
        LOG_WARN("ROOM", "Non-host tried to start game: char " + std::to_string(session->characterId));
        return;
    }
    
    // Check if can start (minimum players, all ready)
    if (!room->canStart()) {
        if (room->playerCount() < 2 && room->settings().mode != GameMode::Tutorial) {
            session->send(PacketBuilder::displayMessage(u"Need more players", 0));
        } else if (!room->areAllPlayersReady()) {
            session->send(PacketBuilder::displayMessage(u"Players not ready", 0));
        }
        LOG_WARN("ROOM", "Cannot start game: canStart=false");
        return;
    }
    
    LOG_INFO("ROOM", "Host " + std::to_string(session->characterId) + 
             " starting game in room " + std::to_string(room->id()));
    
    m_raceHandler.handleStartRace(session, room.get(), this);
}

void GameServer::handlePlayerReady(Session::Ptr session, Packet& packet) {
    bool ready = packet.readUInt8() != 0;
    
    auto room = getRoom(session->roomId);
    if (!room) return;
    
    // Update ready status in room
    room->setPlayerReady(session->id(), ready);
    
    // Get player data for broadcast
    auto* roomPlayer = room->getPlayer(session->id());
    if (!roomPlayer) return;
    
    // Build PlayerData for update packet
    PlayerData pd;
    pd.id = session->characterId;
    for (char16_t c : roomPlayer->name) {
        if (c > 0 && c < 128) pd.name += static_cast<char>(c);
    }
    pd.slot = roomPlayer->slot;
    pd.ready = ready;
    
    // Broadcast ready state to all in room
    room->broadcast(PacketBuilder::playerUpdate(pd));
    
    LOG_INFO("ROOM", "Player " + std::to_string(session->characterId) + 
             " ready=" + std::to_string(ready) + 
             " (ready count: " + std::to_string(room->readyCount()) + "/" + 
             std::to_string(room->playerCount()) + ")");
    
    // Notify host if all players are ready
    if (room->areAllPlayersReady() && room->playerCount() >= 2) {
        for (auto& sess : room->sessions()) {
            if (room->isHost(sess->id())) {
                sess->send(PacketBuilder::displayMessage(u"All players ready!", 1));
                break;
            }
        }
    }
}

// =============================================================================
// SHOP HANDLERS
// =============================================================================

void GameServer::handleShopBrowse(Session::Ptr session, Packet& packet) {
    m_shopHandler.handleBrowse(session, packet, this);
}

void GameServer::handleSellItem(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleSellItem(session, packet, this);
}

// =============================================================================
// INVENTORY HANDLERS
// =============================================================================

void GameServer::handleEquipVehicle(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleEquipVehicle(session, packet, this);
}

void GameServer::handleEquipAccessory(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleEquipAccessory(session, packet, this);
}

void GameServer::handleUseItem(Session::Ptr session, Packet& packet) {
    m_inventoryHandler.handleUseItem(session, packet, this);
}

// =============================================================================
// PLAYER DATA
// =============================================================================

void GameServer::sendPlayerData(Session::Ptr session) {
    LOG_INFO("GAME", "Sending player data to " + session->remoteAddress() + 
             " (account=" + std::to_string(session->accountId) + ")");
    
    auto& db = Database::instance();
    
    // =========================================================================
    // 1. Load character data (all columns)
    // =========================================================================
    auto chars = db.query(
        "SELECT id, name, level, experience, gold, cash, wins, losses, "
        "COALESCE(total_races, 0) AS total_races, "
        "COALESCE(playtime_minutes, 0) AS playtime_minutes, "
        "COALESCE(license_class, 0) AS license_class, "
        "COALESCE(rank_points, 0) AS rank_points, "
        "COALESCE(equipped_driver_id, 1) AS equipped_driver_id, "
        "COALESCE(tutorial_completed, 0) AS tutorial_completed, "
        "COALESCE(is_gm, 0) AS is_gm "
        "FROM characters WHERE account_id = " + std::to_string(session->accountId) + " LIMIT 1"
    );
    
    PlayerData player;
    player.level = 1;
    player.gold = 1000;
    
    if (chars.empty()) {
        LOG_WARN("GAME", "No character found for account " + std::to_string(session->accountId));
        
        // Try to trigger character creation screen
        // Send 0x11 (Menu state) to initialize UI, then 0x03 (character creation)
        Packet menuPkt(CMD::S_SHOW_MENU);  // 0x11
        session->send(menuPkt);
        LOG_INFO("GAME", "Sent SHOW_MENU (0x11) to " + session->remoteAddress());
        
        Packet createPkt(CMD::S_TRIGGER);  // 0x03
        session->send(createPkt);
        LOG_INFO("GAME", "Sent CHARACTER_CREATION (0x03) to " + session->remoteAddress());
        return;
    }
    
    // Parse character data
    player.id = std::stoi(chars[0]["id"]);
    player.accountId = session->accountId;
    player.name = chars[0]["name"];
    player.level = std::stoi(chars[0]["level"]);
    player.xp = std::stoi(chars[0]["experience"]);
    player.gold = std::stoi(chars[0]["gold"]);
    player.cash = std::stoi(chars[0]["cash"]);
    player.wins = std::stoi(chars[0]["wins"]);
    player.losses = std::stoi(chars[0]["losses"]);
    player.totalRaces = std::stoi(chars[0]["total_races"]);
    player.playtimeMinutes = std::stoi(chars[0]["playtime_minutes"]);
    player.licenseClass = std::stoi(chars[0]["license_class"]);
    player.rankPoints = std::stoi(chars[0]["rank_points"]);
    player.driverId = std::stoi(chars[0]["equipped_driver_id"]);
    player.tutorialCompleted = chars[0]["tutorial_completed"] == "1";
    player.isGM = chars[0]["is_gm"] == "1";
    
    session->characterId = player.id;
    
    LOG_INFO("GAME", "Character loaded: " + player.name + 
             " (ID=" + std::to_string(player.id) + 
             " Level=" + std::to_string(player.level) +
             " Gold=" + std::to_string(player.gold) +
             " Cash=" + std::to_string(player.cash) +
             " Wins=" + std::to_string(player.wins) + ")");
    
    // =========================================================================
    // 2. Load equipped vehicle
    // =========================================================================
    auto equippedVeh = db.query(
        "SELECT id, vehicle_type_id FROM vehicles "
        "WHERE character_id = " + std::to_string(player.id) + " AND equipped = 1 LIMIT 1"
    );
    if (!equippedVeh.empty()) {
        player.vehicleId = std::stoi(equippedVeh[0]["id"]);
        player.vehicleTemplateId = std::stoi(equippedVeh[0]["vehicle_type_id"]);
        LOG_DEBUG("GAME", "Equipped vehicle: ID=" + std::to_string(player.vehicleId) + 
                  " Template=" + std::to_string(player.vehicleTemplateId));
    }
    
    // =========================================================================
    // 3. Send connection OK (0x0A) with real player data FIRST
    // =========================================================================
    session->send(PacketBuilder::connectionOkWithPlayer(player));
    LOG_INFO("GAME", "SEND 0x0A CONNECTION_OK (38 bytes with player data)");
    
    // =========================================================================
    // 4. Send display message (0x02) to confirm connection
    // =========================================================================
    session->send(PacketBuilder::displayMessage(u"", 1));
    LOG_INFO("GAME", "SEND 0x02 DISPLAY_MESSAGE (code=1 OK)");
    
    // =========================================================================
    // 5. Send session confirm (0x07/0xA7 with PlayerInfo 1224 bytes)
    // =========================================================================
    session->send(PacketBuilder::sessionConfirm(session->accountId, player));
    LOG_INFO("GAME", "SEND 0xA7 SESSION_CONFIRM (PlayerInfo 1224 bytes)");
    
    // =========================================================================
    // 4. Load and send VEHICLES inventory (0x1B)
    // =========================================================================
    auto vehicles = db.query(
        "SELECT id, vehicle_type_id, durability, max_durability, "
        "COALESCE(stat_speed, 50) AS stat_speed, "
        "COALESCE(stat_accel, 50) AS stat_accel, "
        "COALESCE(stat_handling, 50) AS stat_handling, "
        "COALESCE(stat_drift, 40) AS stat_drift, "
        "COALESCE(stat_boost, 30) AS stat_boost, "
        "COALESCE(stat_weight, 50) AS stat_weight, "
        "COALESCE(stat_special, 0) AS stat_special, "
        "equipped "
        "FROM vehicles WHERE character_id = " + std::to_string(player.id)
    );
    
    std::vector<VehicleInfo> vehicleList;
    for (const auto& v : vehicles) {
        VehicleInfo vi;
        vi.id = std::stoi(v.at("id"));
        vi.templateId = std::stoi(v.at("vehicle_type_id"));
        vi.ownerId = player.id;  // Character ID
        vi.durability = std::stoi(v.at("durability"));
        vi.maxDurability = std::stoi(v.at("max_durability"));
        vi.stats[0] = std::stoi(v.at("stat_speed"));
        vi.stats[1] = std::stoi(v.at("stat_accel"));
        vi.stats[2] = std::stoi(v.at("stat_handling"));
        vi.stats[3] = std::stoi(v.at("stat_drift"));
        vi.stats[4] = std::stoi(v.at("stat_boost"));
        vi.stats[5] = std::stoi(v.at("stat_weight"));
        vi.stats[6] = std::stoi(v.at("stat_special"));
        vi.equipped = v.at("equipped") == "1";
        vehicleList.push_back(vi);
    }
    session->send(PacketBuilder::inventoryVehicles(vehicleList));
    LOG_INFO("GAME", "SEND 0x1B INVENTORY_VEHICLES (" + std::to_string(vehicleList.size()) + " vehicles)");
    
    // =========================================================================
    // 5. Load and send ITEMS inventory (0x1C)
    // =========================================================================
    auto items = db.query(
        "SELECT id, item_type_id, quantity, slot, COALESCE(equipped, 0) AS equipped "
        "FROM items WHERE character_id = " + std::to_string(player.id)
    );
    
    std::vector<ItemInfo> itemList;
    for (const auto& item : items) {
        ItemInfo ii;
        ii.id = std::stoi(item.at("id"));
        ii.templateId = std::stoi(item.at("item_type_id"));
        ii.ownerId = player.id;  // Character ID
        ii.quantity = std::stoi(item.at("quantity"));
        ii.slot = std::stoi(item.at("slot"));
        ii.equipped = item.at("equipped") == "1";
        itemList.push_back(ii);
    }
    session->send(PacketBuilder::inventoryItems(itemList));
    LOG_INFO("GAME", "SEND 0x1C INVENTORY_ITEMS (" + std::to_string(itemList.size()) + " items)");
    
    // =========================================================================
    // 6. Load and send ACCESSORIES inventory (0x1D)
    // =========================================================================
    auto accessories = db.query(
        "SELECT id, accessory_type_id, slot, bonus1, bonus2, bonus3, equipped "
        "FROM accessories WHERE character_id = " + std::to_string(player.id)
    );
    
    std::vector<AccessoryInfo> accessoryList;
    for (const auto& acc : accessories) {
        AccessoryInfo ai;
        ai.id = std::stoi(acc.at("id"));
        ai.templateId = std::stoi(acc.at("accessory_type_id"));
        ai.slot = std::stoi(acc.at("slot"));
        ai.bonus1 = std::stoi(acc.at("bonus1"));
        ai.bonus2 = std::stoi(acc.at("bonus2"));
        ai.bonus3 = std::stoi(acc.at("bonus3"));
        ai.equipped = acc.at("equipped") == "1";
        accessoryList.push_back(ai);
    }
    session->send(PacketBuilder::inventoryAccessories(accessoryList));
    LOG_INFO("GAME", "SEND 0x1D INVENTORY_ACCESSORIES (" + std::to_string(accessoryList.size()) + " accessories)");
    
    // =========================================================================
    // 7. Update last_played in DB
    // =========================================================================
    db.execute("UPDATE characters SET last_played = NOW() WHERE id = " + std::to_string(player.id));
    
    // =========================================================================
    // 8. Check tutorial completion - NEW PLAYERS GO TO TUTORIAL!
    // =========================================================================
    if (!player.tutorialCompleted) {
        // New player - send to TutorialMenu (state 14) instead of lobby
        // CMD 0x16 -> sub_47E8B0 -> state 14 (TutorialMenu)
        Packet tutorialMenu(CMD::S_UI_STATE_14);
        // 0x16 handler reads NO data - just triggers state change
        session->send(tutorialMenu);
        LOG_INFO("GAME", "SEND 0x16 UI_STATE_14 (Tutorial required - tutorial_completed=0)");
        return;
    }
    
    // =========================================================================
    // 9. Send lobby UI (0x12) + room list (0x3F) - Only for players with completed tutorial
    // =========================================================================
    Packet showLobby(CMD::S_SHOW_LOBBY);
    session->send(showLobby);
    LOG_INFO("GAME", "SEND 0x12 SHOW_LOBBY");
    
    // send room list (0x3F) - client reads only int32 count
    Packet roomList(CMD::S_ROOM_INFO);
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        roomList.writeInt32(static_cast<int32_t>(m_rooms.size()));  // Just the count
    }
    session->send(roomList);
    LOG_INFO("GAME", "SEND 0x3F ROOM_LIST (count=" + std::to_string(m_rooms.size()) + ")");
}

// =============================================================================
// SESSION/ROOM MANAGEMENT
// =============================================================================

void GameServer::onDisconnect(Session::Ptr session) {
    LOG_INFO("GAME", "Client disconnected: " + session->remoteAddress() + " (ID: " + std::to_string(session->id()) + ")");
    
    // remove from room
    auto room = getRoom(session->roomId);
    if (room) {
        room->removePlayer(session->id());
        if (!room->isEmpty()) {
            room->broadcast(PacketBuilder::playerLeft(session->characterId));
        }
    }
    
    // clean empty rooms
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        for (auto it = m_rooms.begin(); it != m_rooms.end(); ) {
            if (it->second->isEmpty()) {
                LOG_INFO("GAME", "Removing empty room: " + std::to_string(it->first));
                it = m_rooms.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    removeSession(session->id());
}

std::shared_ptr<Room> GameServer::createRoom(const RoomSettings& settings) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    auto room = std::make_shared<Room>(m_nextRoomId++, settings);
    m_rooms[room->id()] = room;
    LOG_INFO("GAME", "Created room: " + settings.name + " (ID: " + std::to_string(room->id()) + ")");
    return room;
}

std::shared_ptr<Room> GameServer::getRoom(uint32_t roomId) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    auto it = m_rooms.find(roomId);
    return (it != m_rooms.end()) ? it->second : nullptr;
}

void GameServer::removeRoom(uint32_t roomId) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);
    m_rooms.erase(roomId);
}

void GameServer::addSession(Session::Ptr session) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions[session->id()] = session;
}

void GameServer::removeSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions.erase(sessionId);
}

Session::Ptr GameServer::getSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second : nullptr;
}

// =============================================================================
// DATA HANDLERS
// =============================================================================

void GameServer::handleRequestData(Session::Ptr session, Packet& packet) {
    // CMD 0x4D - Client requests 276 bytes of data
    // Payload: 276 bytes request data
    if (packet.remaining() < 276) {
        LOG_WARN("GAME", "REQUEST_DATA too small from " + session->remoteAddress());
        return;
    }
    
    // Read request type (first 4 bytes usually indicate what data is requested)
    uint32_t requestType = packet.readUInt32();
    
    LOG_DEBUG("GAME", "REQUEST_DATA type=" + std::to_string(requestType) + " from " + session->remoteAddress());
    
    // Respond based on request type - for now send acknowledgment
    // Actual implementation depends on what data the client expects
}

void GameServer::handleUnknown32(Session::Ptr session, Packet& packet) {
    // CMD 0x32 - Unknown purpose (8 bytes)
    // Possibly some state sync or timing data
    if (packet.remaining() < 8) {
        return;
    }
    
    uint32_t data1 = packet.readUInt32();
    uint32_t data2 = packet.readUInt32();
    
    LOG_DEBUG("GAME", "CMD 0x32: " + std::to_string(data1) + ", " + std::to_string(data2) + 
              " from " + session->remoteAddress());
    
    // No response needed - appears to be informational
}

} // namespace knc
