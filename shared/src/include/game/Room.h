
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>

namespace knc {

class Player;
class Session;

enum class RoomState : uint8_t {
    Waiting = 0,
    Starting = 1,
    Loading = 2,
    Racing = 3,
    Results = 4
};

// icon is base plus mode times 0x130 at 0x616C team change at 0x40E2D1 only for modes 1 and 3
enum class GameMode : uint8_t {
    ItemSingle = 0,
    ItemTeam   = 1,
    SpeedSingle = 2,
    SpeedTeam  = 3,
    Battle     = 4
};

struct RoomSettings {
    std::string name = "Room";
    std::string password;
    GameMode mode = GameMode::ItemSingle;
    uint8_t maxPlayers = 8;
    uint8_t mapId = 1;
    uint8_t laps = 3;
    bool isPrivate = false;
    bool teamMode = false;
    bool fillWithBots = false;    // opt in top up to seat cap read by fillWithBotsIfEnabled
};

struct RoomPlayer {
    uint32_t sessionId = 0;
    int32_t characterId = 0;
    std::u16string name;
    uint8_t slot = 0;
    uint8_t team = 0;             // slot 0 to 7 team 0 none 1 red 2 blue
    bool ready = false;
    bool isHost = false;
    int32_t vehicleTemplateId = 0;
    int32_t driverId = 1;
    bool isBot = false;           // CPU filler no session no DB
};

class Room {
public:
    // bots use a reserved id range above 0x40000000 above any db id but small enough for client member tables
    static constexpr int32_t kBotIdBase = 900000;
    // 1001 legacy starter has no model on disk 10010 basic 1 ships both the car and body files
    static constexpr int32_t kBotVehicleTemplateId = 10010;
    // driver 1 racer basic has no asset driver 10 pumpkin ships a full bodyset
    static constexpr int32_t kBotDriverId = 10;
    static constexpr uint8_t kMaxGridSlots = 16;
    // no seat left sub 40D650 refuses a second member on a taken slot so never hand one out twice
    static constexpr uint8_t kNoSlot = 0xFF;
    static bool isBotId(int32_t id) { return id >= kBotIdBase; }

    Room(uint32_t id, const RoomSettings& settings);

    uint32_t id() const { return m_id; }
    const std::string& name() const { return m_settings.name; }
    RoomState state() const { return m_state; }
    const RoomSettings& settings() const { return m_settings; }
    RoomSettings& settings() { return m_settings; }
    
    bool addPlayer(std::shared_ptr<Session> session, int32_t characterId, 
                   const std::u16string& name, int32_t vehicleTemplateId);
    bool addPlayer(std::shared_ptr<Session> session);  // Legacy compatibility
    void removePlayer(uint32_t sessionId);
    // humans plus bots so isFull and lobby count see a full grid one row per seat
    size_t playerCount() const { return m_players.size() + m_bots.size(); }
    size_t humanCount() const { return m_players.size(); }
    bool isFull() const { return playerCount() >= seatCap(); }
    /// seats the room really has the wire grid never holds more than kMaxGridSlots
    uint8_t seatCap() const {
        return m_settings.maxPlayers < kMaxGridSlots ? m_settings.maxPlayers : kMaxGridSlots;
    }
    bool isEmpty() const { return m_sessions.empty(); }

    RoomPlayer* getPlayer(uint32_t sessionId);
    const RoomPlayer* getPlayer(uint32_t sessionId) const;
    RoomPlayer* getPlayerByCharacter(int32_t characterId);  // searches humans then bots
    std::vector<RoomPlayer> getPlayers() const;

    /// nowMs when the results board went out zero once the room screen returns
    int64_t resultsShownMs = 0;

    int addBots(int count);
    int fillWithBotsIfEnabled();  // tops the room to its seat cap with bots when settings opted in
    void clearBots();
    size_t botCount() const { return m_bots.size(); }
    const std::vector<RoomPlayer>& bots() const { return m_bots; }
    std::vector<RoomPlayer> participants() const;  // humans plus bots by slot
    
    void setPlayerReady(uint32_t sessionId, bool ready);
    bool isPlayerReady(uint32_t sessionId) const;
    bool areAllPlayersReady() const;  // Excludes host
    int readyCount() const;
    
    void setPlayerTeam(uint32_t sessionId, uint8_t team);
    // modes 1 and 3 seat every member on a side the client counts red or blue from the 0x21
    bool isTeamMode() const { return m_settings.mode == GameMode::ItemTeam || m_settings.mode == GameMode::SpeedTeam; }
    // the side with fewer members humans and bots together red on a tie
    uint8_t balancedTeam() const;
    
    uint32_t hostSessionId() const { return m_hostSessionId; }
    uint32_t hostId() const { return m_hostSessionId; }  // Alias for compatibility
    int32_t hostCharacterId() const;
    void setHost(uint32_t sessionId);
    bool isHost(uint32_t sessionId) const { return m_hostSessionId == sessionId; }
    
    /// first seat no human and no bot holds kNoSlot when the room is full
    uint8_t findEmptySlot() const;
    uint8_t nextFreeSlot() const;   // same allocator kept for older call sites
    
    void setState(RoomState state);
    /// nullptr when the master may start else the client message key of the rule that failed
    const char* startRefusal() const;
    bool canStart() const { return startRefusal() == nullptr; }
    bool isPlaying() const { return m_state == RoomState::Racing || m_state == RoomState::Loading; }
    bool isWaiting() const { return m_state == RoomState::Waiting; }
    
    void broadcast(const class Packet& packet);
    void broadcastExcept(const class Packet& packet, uint32_t excludeSessionId);
    
    const std::vector<std::shared_ptr<Session>>& sessions() const { return m_sessions; }

private:
    uint32_t m_id;
    RoomSettings m_settings;
    RoomState m_state = RoomState::Waiting;
    uint32_t m_hostSessionId = 0;
    
    std::vector<std::shared_ptr<Session>> m_sessions;
    std::unordered_map<uint32_t, RoomPlayer> m_players;  // sessionId maps to RoomPlayer
    std::vector<RoomPlayer> m_bots;
};

}

