// game logic rooms races dispatch

#pragma once
#include <asio.hpp>
#include <array>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "net/Session.h"
#include "net/Protocol.h"
#include "net/ListenerHealth.h"
#include "db/DbWorkerPool.h"
#include "security/RateLimiter.h"
#include "game/Room.h"
#include "packets/PacketBuilder.h"
#include "handlers/ChatHandler.h"
#include "handlers/ShopHandler.h"
#include "handlers/RaceHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/SocialHandler.h"
#include "handlers/CarCraftHandler.h"
#include "handlers/RoomCraftHandler.h"

namespace knc {

class GameServer {
public:
    explicit GameServer(int port);
    
    void run();
    void stop();

    /// true while the accept loop still turns the listening port alone never proves it
    bool listenerAlive(int64_t maxAgeMs = 30000) const;
    /// file the container probe reads set from the ini or the environment
    void setLivenessFile(const std::string& path) { m_listenerHealth.setFile(path); }
    /// where the S2C 0x0019 channel return sends the client back the exe reauths there with 0x00A7
    void setLoginEndpoint(const std::string& host, int port) { m_loginHost = host; m_loginPort = port; }

    std::shared_ptr<Room> createRoom(const RoomSettings& settings);
    std::shared_ptr<Room> getRoom(uint32_t roomId);
    void removeRoom(uint32_t roomId);
    const std::unordered_map<uint32_t, std::shared_ptr<Room>>& rooms() const { return m_rooms; }

    void addSession(Session::Ptr session);
    void removeSession(uint32_t sessionId);
    Session::Ptr getSession(uint32_t sessionId);
    Session::Ptr findSessionByCharacterId(int32_t characterId) const;
    size_t sessionCount() const { return m_sessions.size(); }

    /// queues db work off io thread one key per job in order false return means caller must do it itself
    bool postDb(uint64_t orderKey, std::function<void()> job);
    /// hands a result back to the io loop Session send has no lock so only that thread may send
    void postIo(std::function<void()> job);

    // exposes the asio io context so timer handlers like countdown and throttled broadcasts share the recv thread
    asio::io_context& ioContext() { return m_ioContext; }

    /// the shop the gacha and the rewards push a new chassis loadout through it
    CarCraftHandler& carCraft() { return m_carCraftHandler; }
    
    // all sessions for lobby broadcast
    std::vector<Session::Ptr> getSessions() const {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        std::vector<Session::Ptr> result;
        result.reserve(m_sessions.size());
        for (const auto& [id, session] : m_sessions) {
            result.push_back(session);
        }
        return result;
    }
    
    // sessions in lobby not in any room
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
    /// greets one accepted socket a throw here must never reach the loop
    void acceptOne(asio::ip::tcp::socket socket);
    void handlePacket(Session::Ptr session, Packet& packet);
    void onDisconnect(Session::Ptr session);
public:
    /// everything the login burst builds off the io thread before one frame leaves
    struct LoginBurstFrames {
        std::vector<Packet> preBurst;               ///< shop then room craft frames sent first then chunks follow ordered for the spaced sender
        std::vector<std::vector<uint8_t>> chunks;
        size_t totalBytes = 0;
        size_t chunkCount = 0;
        double dbMs = 0.0;                          ///< what the worker spent so the log can split the cost
    };

    /// sends catalogs once then player data or the creation popup writes session fields and hands the read to db pool
    void sendPlayerData(Session::Ptr session);
    /// pool side builds every frame of the burst touches no Session and no Room
    void collectLoginBurst(const Session::Ptr& session, const PlayerData& player,
                           bool creating, bool catalogs, LoginBurstFrames& out);
    /// io side sends the lead frames then drips the chunks and arms the screen ack
    void dripLoginBurst(const Session::Ptr& session, const PlayerData& player,
                        bool creating, LoginBurstFrames& frames);
private:

    void handleHeartbeat(Session::Ptr session, Packet& packet);
    void handleClientAuth(Session::Ptr session, Packet& packet);
    void handleFullState(Session::Ptr session, Packet& packet);
    void handleClientInfo(Session::Ptr session, Packet& packet);
    void handleSessionConfirm(Session::Ptr session, Packet& packet);
    /// the account is bound to this socket now an older socket on it is dropped and online players says so
    void bindAccount(Session::Ptr session, uint32_t accountId, uint32_t characterId);

    void handleChannelSelect(Session::Ptr session, Packet& packet);
    void handleLobbyRequest(Session::Ptr session, Packet& packet);
    // 0x2D per room the lobby grid is empty until these arrive
    void sendLobbyRoomList(const std::shared_ptr<Session>& session);
    // walking to the lobby leaves the room too since the client only sends 0x12 not 0x22 or seats leak
    void leaveCurrentRoom(const std::shared_ptr<Session>& session);
    // tell everyone parked in the lobby that a room died
    void broadcastLobbyRoomRemove(uint32_t roomId);
    void handleServerQuery(Session::Ptr session, Packet& packet);

    void handleCreateRoom(Session::Ptr session, Packet& packet);
    void handleJoinRoom(Session::Ptr session, Packet& packet);
    void handleLeaveRoom(Session::Ptr session, Packet& packet);

    // 0x2D is create room not chat see handleCreateRoom
    void handleWhisper(Session::Ptr session, Packet& packet);
    void handleLobbyChat(Session::Ptr session, Packet& packet);

    void handleStateChange(Session::Ptr session, Packet& packet);
public:
    /// seats the player in a waiting room of that mode or opens one then sends the join chain
    void quickMatch(Session::Ptr session, int32_t mode);
private:
    void handleRoomKick(Session::Ptr session, Packet& packet);     // 0x39
    void handleGameStart(Session::Ptr session, Packet& packet);
    void handlePlayerReady(Session::Ptr session, Packet& packet);
    // C2S 0x64 in room team change 0 red 1 blue team modes 1 and 3 only
    void handleTeamChange(Session::Ptr session, Packet& packet);

    void handleSellItem(Session::Ptr session, Packet& packet);

    void handleRequestData(Session::Ptr session, Packet& packet);
    void handleUnknown32(Session::Ptr session, Packet& packet);

    void handleEquipVehicle(Session::Ptr session, Packet& packet);
    void handleEquipAccessory(Session::Ptr session, Packet& packet);
    void handleUseItem(Session::Ptr session, Packet& packet);
    
    asio::io_context m_ioContext;
    asio::steady_timer m_raceTickTimer{m_ioContext};
    void startRaceTick();

    /// seats a bot after kMatchWaitSeconds since the client refuses a room of one with MSG NOT ALONE
    asio::steady_timer m_matchTickTimer{m_ioContext};
    void startMatchTick();

    /// refreshes the liveness stamp from inside the loop so a frozen loop reads as deaf
    asio::steady_timer m_livenessTimer{m_ioContext};
    void startLivenessTick();
    /// arms the acceptor again after a descriptor shortage instead of spinning hot
    asio::steady_timer m_acceptRetryTimer{m_ioContext};
    ListenerHealth m_listenerHealth;
    int m_livenessTicks = 0;
    // one slow query in a handler used to stall every client the pool takes that work now
    DbWorkerPool m_dbPool;
    std::string m_loginHost;
    int m_loginPort = 0;

public:
    /// fills wire blobs the room and race grid need sub 490A70 drops the client if a kart misses 0xC2
    static void loadoutBlobs(const RoomPlayer& rp,
                             std::array<uint8_t, 0x2C>& character,
                             std::array<uint8_t, 0x38>& kart,
                             std::array<uint8_t, 0x3C>& customCar);

private:
    static constexpr int kMatchWaitSeconds = 20;   // seats a CPU car after this wait one more added each cycle up to three
    static constexpr size_t kMatchMaxBots = 3;
    /// the stock waiting box of FUN 00410640 clears itself after this so the CPU car waits the same
    static constexpr int64_t kRandomInviteWaitMs = 5000;

    /// one human for a 0x12F an idle lobby session first then a waiting room of another room
    Session::Ptr pickRandomInviteTarget(const Session::Ptr& asker, Room* room) const;
    /// seats one CPU car and sends the 0x32 then 0x21 pair false when no seat was free
    bool seatOneBot(Room* room, const char* why);

    /// the join chain 0x13 then the 0x32 slots then one 0x21 per member then 0x30 and 0x35
    void sendRoomScreen(const std::shared_ptr<Session>& session, Room* room);
    /// the peers learn a joiner slot first then member
    void announceRoomJoin(const std::shared_ptr<Session>& session, Room* room);

    /// drips a burst like the login stream the client buffer is a fixed 0x2000 in sub 476CC0 or it overflows
public:
    void sendDripped(const std::shared_ptr<Session>& session, std::vector<Packet> packets);
private:

    // podium plus rest counter run about twenty seconds on the client
    static constexpr int64_t kResultsBoardMs = 21000;
    std::map<uint32_t, int64_t> m_roomWaitSince;
    // room id to the ms a 0x12F invite stops waiting for the human and takes a CPU car
    std::map<uint32_t, int64_t> m_randomInviteDue;
    asio::ip::tcp::acceptor m_acceptor;
    
    std::unordered_map<uint32_t, Session::Ptr> m_sessions;
    // per session flood throttle same key as m sessions guarded by m sessions mutex
    std::unordered_map<uint32_t, RateLimiter> m_rateLimiters;
    mutable std::mutex m_sessionsMutex;
    std::unordered_map<uint32_t, std::shared_ptr<Room>> m_rooms;
    mutable std::mutex m_roomsMutex;
    uint32_t m_nextRoomId = 1;

    ChatHandler m_chatHandler;
    ShopHandler m_shopHandler;
    RaceHandler m_raceHandler;
    InventoryHandler m_inventoryHandler;
    SocialHandler m_socialHandler;
    CarCraftHandler m_carCraftHandler;
    RoomCraftHandler m_roomCraftHandler;
};

} // namespace knc
